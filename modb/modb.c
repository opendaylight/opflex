/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "gendcls.h"
#include "coverage.h"
#include "config-file.h"
#include "fnm-util.h"
#include "meo-util.h"
#include "pag-thread.h"
#include "seq-util.h"
#include "hash-util.h"
#include "tv-util.h"
#include "dirs.h"
#include "modb.h"
#include "eventq.h"
#include "dbug.h"


/*
 * modb - Managed Object Database
 *
 * This represent the main memory that is the central part of the
 * of the policy agent. Policys, once rendered are stored herein
 * and the state of the device layer is also represent herein.
 *
 * History:
 *    09-APR-2014 dkehn@noironetworks.com - created.
 *
 */

/* 
 * modb dcls
 */
static pthread_t modb_tid;
static FILENAME modb_fname;

/*
 * config definitions:
 *  crash_recovery: (true/false): true we look for the $dbdir/modb.dat
 *       file and recover from it.
 * checkpoint_method: (acid/time): the method for writing to the modb.dat
 *       file. acid means we fsync upon detection that something has changed
 *       in the memory, this ensures complete recovery. If time, we checkpoint
 *       every checkpoint_interval seconds, wheather we need it or not. At 
 *       most we lost checkpoint_interval seconds of changes.
 * checkpoint_interval: (seconds):  number of seconds between checkpoints, if
 *       checkpoint_method == time, else meaningless.
 * hash_debug: (true/false): turns on the hash_table debugging.
 * hash_size: (cells): the number of hash indexes to create.
 * mod_debug: (true/false): turns on debugging on th modb.
 */


/* config file defaults, this exists such if ther is no definition in
 * the config file then the config option will defualt to what is 
 * defined here.
 */
static struct option_ele modb_config_defaults[] = {
    {MODB_SECTION, "crash_recovery", "false"},
    {MODB_SECTION, "checkpoint_method", "acid"},
    {MODB_SECTION, "checkpoint_interval", "1"},
    {MODB_SECTION, CLASS_IDX_SZ_TAG, "4000"},
    {MODB_SECTION, NODE_IDX_SZ_TAG, "4000"},
    {MODB_SECTION, URI_IDX_SZ_TAG, "4000"},
    {MODB_SECTION, URI_ATTR_IDX_SZ_TAG, "8000"},
    {MODB_SECTION, "mod_debug", "false"},
    {NULL, NULL, NULL}
};

/* Index lookups
*/
static struct hash_index class_id_shash = {0};
static struct hash_index node_id_shash = {0};
static struct hash_index uri_shash = {0};
static struct hash_index uri_attr_shash = {0};

static hash_init_t index_init_list[] = { 
    {NODE_INDEX_NAME, &node_id_shash},
    {CLASS_INDEX_NAME, &class_id_shash},
    {URI_INDEX_NAME, &uri_shash},
    {URI_ATTR_INDEX_NAME, &uri_attr_shash},
    {NULL, NULL}
};

/* node list head - this is the head which points to the node_list. The lock 
 *    herein will lock the entire node_list (each node_list has locks as well
 *    updating attributes), which should be use for updating the node_list.
 */
static head_list_p node_list = NULL;

/* This must match the enum_head_state */
static const char *head_state_msg[] = {
    "CHECKPOINT_STARTED",
    "CHECKPOINT_COMPLETE",
    "INITIALIZED",
    "DESTROYING",
    "LOADED"
};

/* This must match the enum_node_state */
static const char *node_state_msg[] = {
    "UNINITIALIZED",
    "INSERT",
    "PENDING_UPDATE",
    "PENDING_EVENT",
    "PENDING_ENFORCEMENT",
    "ENFORNCEMENT_SENT",
    "PENDING_TRIGGER",
    "TRIGGER_SENT",
    "UPTODATE",
    "INDEX_ERROR",
    "LIST_ADD_ERROR",
    "CORRUPT"
};

static const char *enforcement_state_msg[] = {
    "NO_EVENT",
    "PENDING",
    "COMPLETE",
    "FAILURE",
    "CHANGE",
    "UPDATED"
};



/* 
 * Protos dcls
 */
static bool head_list_create(head_list_p *hdp);
static bool head_list_destroy(head_list_p *hp, bool force);
uint32_t node_list_insert(head_list_p hdp, node_ele_p ndp);
static bool node_list_free_all(head_list_p hp);
static bool find_ndp_func(void *item, void *dp);
static bool node_list_delete(head_list_p hdp, node_ele_p ndp, int extent, int *rows);
static void node_ele_destroy(node_ele_p *ndp);
static const char *opcode_to_string(int opc);


/* Attr protos */
static bool attr_list_free_all(head_list_p *ahdp);
static void attr_ele_destroy(attribute_p *attrp);
static bool attr_add_to_node(node_ele_p ndp, attribute_p attrp);


/* 
 * Index protos 
*/
static bool index_create(hash_index_p hip, char *name);
static node_ele_p index_get_ptr(hash_init_p hash_initp, void *idp, int itype);
static bool index_destroy(hash_index_p hip);
static bool index_add_node_to_all(hash_init_p hash_initp, node_ele_p ndp);
static bool index_delete_node_from_all(hash_init_p hash_initp, node_ele_p ndp);
static bool index_add_node(hash_index_p hip, node_ele_p ndp, char *key);
static void index_dump(hash_init_p iilp, bool index_node_dump);



/******************************************************************************
 * MODB code 
 *****************************************************************************/

/* modb_initialize - intialize the internal memory and setup the thread.
 *
 * where:
 *   returnes:
 */
bool modb_initialize(void)
{
    static char *mod= "modb_initialize";
    bool retb = 0;
    hash_init_p iilp = NULL;

    //    DBUG_PUSH("d:t:i:L:n:P:T:0");
    DBUG_PROCESS("modb");

    DBUG_ENTER(mod);

    modb_tid = pthread_self();
    conf_initialize(modb_config_defaults);

    /* Initialize the indexes */
    for (iilp = index_init_list; iilp->name != NULL; iilp++) {
        index_create(iilp->hidx, iilp->name);
    }

    /* Create the node head.   */
    head_list_create(&node_list);

    /* crash recovery? */
    if (strcasecmp(conf_get_value(MODB_SECTION, "crash_recovery"), "true") == 0) {
        modb_fname = fnm_create(".dat", MODB_FNAME, pag_dbdir(), NULL);
        modb_crash_recovery(fnm_path(modb_fname));
    }
    
    DBUG_RETURN(retb);
}

/* modb_get_state - This is an accessor for the head state.
 *
 * @returns: <state>       - O
 * 
 */
enum_head_state modb_get_state(void)
{
    static char *mod = "modb_get_state";
    enum_head_state state;

    DBUG_ENTER(mod);
    pag_rwlock_rdlock(&node_list->rwlock);
    state = node_list->state;
    pag_rwlock_unlock(&node_list->rwlock);

    DBUG_RETURN(state);
}

/* modb_op - operations on the DB.
 * 
 * where:
 * @param0 <operation>     - I
 *         this will one of the possible enum_q_op_code.
 * @param1 <dp>            - I
 *         this is defined by the itype, it can be a 
 *         NULL, node_id, Class_id, URI and it will point to an array
 *         defined by count. NOTE: for insert the dp must point to
 *         node_ele_t *.
 * @param1 <itype.         - I
 *         itype defines the nature of the dp data and the index
 *         that should be used to resolve the dp.
 * @param3 <count>         - I
 *         count define how many dp are pointing to, CURRENTLY only set to 1.
 * @param4 <extent>        - I
 *         the extent of the query operation, how much will be effected.
 * @param5 <resultp>       - O
 *         See the structure definition in modb.h, but basically the results
 *         from the operation.
 * @returns: 0 = success or resultp->ret_code.
 */
int modb_op(int operation, void *dp, int itype, int dp_count,
                         int extent, result_p resultp)
{
    static char *mod = "modb_op";
    node_ele_p ndp;
    uint32_t id;
    int i;
    struct timeval start = tv_tod();
    struct timeval elapsed;

    DBUG_ENTER(mod);
    DBUG_PRINT("DEBUG", ("op=%s",opcode_to_string(operation)));

    memset(resultp, 0, sizeof(result_t));
    resultp->ret_code = OP_RC_SUCCESS;
    resultp->op = operation;
    memset(resultp->err_msg, 0, ERR_MSG_SZ);

    switch (operation) {
    case OP_INSERT:
        for (i=0, ndp=(node_ele_p)dp; i < dp_count; ndp++, i++) {
            switch (extent) {
            case EXT_NODE:
                id = node_list_insert(node_list, ndp);
                if (id == 0) {
                    resultp->ret_code = OP_RC_ERROR;
                    sprintf(resultp->err_msg,"%s: Error inserting node inserted:%d: outof:%d", 
                            mod, resultp->rows, dp_count);
                    DBUG_PRINT("ERROR", (resultp->err_msg));
                    break;
                }
                break;
            case EXT_FULL:
                /* TODO dkehn: implementation required */
                break;
            case EXT_NODE_AND_ATTR:
                /* TODO dkehn: implementation required */
                break;
            default:
                break;
            }
            resultp->rows = i;
        }
        break;
    case OP_DELETE:
        switch (itype) {
        case IT_NODE:
            for (i=0; i < dp_count; i++) {                
                ndp = (node_ele_p)(dp+i);
                node_list_delete(node_list, ndp, extent, &resultp->rows);
                if (!resultp->rows) {
                    resultp->ret_code = OP_RC_ERROR;
                    sprintf(resultp->err_msg,"%s: Erorr deleting nodes: %d",
                            mod, resultp->rows);
                    goto rtn_return;
                }
            }
            /* the dp is pointing at an array of node_ids */
            resultp->ret_code = OP_RC_SUCCESS;
            break;
        case IT_NODE_INDEX:
            for (i=0; i < dp_count; i++) {                
                ndp = index_get_ptr(index_init_list, (dp+i), itype);
                node_list_delete(node_list, ndp, extent, &resultp->rows);
                if (!resultp->rows) {
                    resultp->ret_code = OP_RC_ERROR;
                    sprintf(resultp->err_msg,"%s: Erorr deleting nodes: %d",
                            mod, resultp->rows);
                    goto rtn_return;
                }
            }
            /* the dp is pointing at an array of node_ids */
            resultp->ret_code = OP_RC_SUCCESS;
            break;
        case IT_CLASS_ID_INDEX:
            /* the dp is pointing at an array of class_ids, bulk delete */
            resultp->ret_code = OP_RC_SUCCESS;
            break;
        case IT_URI_INDEX:
            /* the dp is pointing at an array of node_ids */
            resultp->ret_code = OP_RC_SUCCESS;
            break;
        default:
            /* the dp is pointing at an array of node_ele_p */
            break;
        }
        break;
    case OP_SELECT:
        break;
    case OP_UPDATE:
        break;
    case OP_NOP:
        /* we do nothing */
        resultp->ret_code = OP_RC_SUCCESS;
        break;
    default:
        resultp->ret_code = OP_RC_ERROR;
        sprintf(resultp->err_msg, "%s: Unreconized operation code: %d", 
                mod, operation);
        DBUG_PRINT("ERROR", (resultp->err_msg));
        break;
    }

 rtn_return:
    elapsed = tv_subtract(tv_tod(), start);
    memset(resultp->elapsed, 0, ELAPSED_TIME_SZ);
    sprintf(resultp->elapsed,"%s secs.", tv_show(elapsed, false, NULL));
    DBUG_RETURN(resultp->ret_code);
}

/* opcode_to_string - converts the operation code to string.
 */
static const char *opcode_to_string(int opc)
{
    static char *opstring[] = {
        "NOP",
        "OP_INSERT",
        "DELETE",
        "SELECT",
        "UPDATE",
    };
    return(opstring[opc]);
}

/* modb_cleanup - free up the indexes and MODB data.
 *    
 * TODO dkehn: need to persist the data to disk.
 *
 * where
 */
void modb_cleanup(void)
{
    static char *mod = "modb_cleanup";
    hash_init_p iilp = NULL;

    DBUG_ENTER(mod);

    /* remove the indexes */
    for (iilp = index_init_list; iilp->name != NULL; iilp++) {
        index_destroy(iilp->hidx);
    }

    DBUG_VOID_RETURN;
}

/* modb_dump - this is a diagnostic tool for dumping the contents of the
 *     node list and indexes.
 *
 * where:
 * @param0 <index_node_dump>           - I
 *         dump the nodes that the index point to.
 *
 */
void modb_dump(bool index_node_dump)
{
    static char *mod = "modb_dump";
    node_ele_p ndp = NULL;
    size_t count = 0;
    int i;

    DBUG_ENTER(mod);
    pag_rwlock_rdlock(&node_list->rwlock);
    count = (size_t)list_length(node_list->list);

    /* dump the head */
    DBUG_PRINT("INFO", ("*************************************"));
    DBUG_PRINT("INFO", ("***** MODB DUMP *********************"));
    DBUG_PRINT("INFO", ("*************************************"));
    DBUG_PRINT("INFO", (" State:%s", head_state_msg[node_list->state]));
    DBUG_PRINT("INFO", (" head count:%d   actual:%d", node_list->num_elements,
                        (int)count));
    
    /* Dump the node data */
    for (i=1; i <= count; i++) {
        ndp = list_get(node_list->list, i);
        dump_node(ndp);
    }
    pag_rwlock_unlock(&node_list->rwlock);

    index_dump(index_init_list, index_node_dump);
    DBUG_VOID_RETURN;
}

/* 
 * modb_crash_recovery - restore modb from file.
 *
 * where
 *    <dbfile>     - I
 *       specifics the filename only, will assume the dbdir as the path.
 *       NOTE: only the path is an option form the config file.
 *    <retb.       - O
 *       0 - nothing was loaded, either the file doesn't exists or load error,
 *       in the case of a load error, modb is uneffected.
 *       1 - indicates the modb has been restored.
 */
bool modb_crash_recovery(const char *dbfile)
{
    static char *mod = "modb_crash_recovery";
    bool retb = 0;
    char *dbpath;
    FILENAME dbfname = {0};
    
    DBUG_ENTER(mod);
    DBUG_PRINT("DEBUG", ("dbfile=%s", dbfile));
    
    dbpath = (char *)pag_dbdir();
    if (fnm_exists(dbfname)) {
        DBUG_PRINT("DEBUG" , ("%s: loading: %s\n", mod, fnm_path(dbfname)));

        /* TODO: dkehn, must build out the crash recovery */
        retb = 0;
    }

    DBUG_RETURN(retb);
}

/* ****************************************************************************
 * Local routines.
 *****************************************************************************/

/* *******************************************************/
/* ******** node_list routines ***************************/
/* *******************************************************/

/*
 * head_list_create - 
 * where:
 * param0 <hp>         - I
 *        point to the head of the list.
 * retruns <retb>      - O
 *        0 if successful, else not.
 */
static bool head_list_create(head_list_p *hdp) 
{
    static char *mod = "head_list_create";
    bool retb = false;

    DBUG_ENTER(mod);
    *hdp = xzalloc(sizeof(head_list_t));
    pag_rwlock_init(&(*hdp)->rwlock);
    pag_rwlock_wrlock(&(*hdp)->rwlock);
    (*hdp)->num_elements = 0;
    (*hdp)->list = NULL;
    head_change_state((*hdp), HD_ST_INITIALIZED);
    sequence_init(&(*hdp)->sequence, DEFAULT_SEQ_START_NUMBER,
                  DEFAULT_SEQ_INC);

    pag_rwlock_unlock(&(*hdp)->rwlock);

    DBUG_RETURN(retb);
}

/* node_list_delete - this uses nopde_ele_p to delete a node and based
 *  upon the extent its children.
 *
 * where:
 * @param0 <hdp>               - I
 *          node_list head pointer
 * @param0 <ndp>             - I
 *         noed_ele_p
 * @param2 <extent>            - I
 *         delete just the node or node and properties
 *         or node/propoerties/descendeds.
 * @return <retb>              - I
 *        0 if successful, else not.
 *         
 */
static bool node_list_delete(head_list_p hdp, node_ele_p ndp, int extent, int *rows)
{
    static char *mod = "node_list_delete";
    bool retb = false;
    node_ele_p dp, cndp;
    bool complete = false;
    int lpos, node_deleted, rcnt;
    head_list_p child_hdp, attr_hdp;
    
    DBUG_ENTER(mod);
    node_deleted = 0;
    while (!complete) {
        pag_rwlock_wrlock(&hdp->rwlock);
        lpos = list_find(hdp->list, ndp, find_ndp_func);
        dp = list_delete(&hdp->list, lpos);
        hdp->num_elements--;
        pag_rwlock_unlock(&hdp->rwlock);

        node_deleted++;

        pag_rwlock_wrlock(&dp->rwlock);
        dp->state = ND_ST_DELETE;
        child_hdp = dp->child_list;
        attr_hdp = dp->properties_list;
        attr_list_free_all(&attr_hdp);
        index_delete_node_from_all(index_init_list, dp);

        switch (extent) {
        case EXT_FULL:
            if (ndp->child_list == NULL) {
                complete = true;
            } else {
                /* delete all the children associated to this node. */
                pag_rwlock_wrlock(&child_hdp->rwlock);
                while (list_length(child_hdp->list)) {
                    cndp = list_delete(&child_hdp->list, 1);
                    node_list_delete(child_hdp, cndp, extent, &rcnt);
                    node_deleted += rcnt;
                }
                head_list_destroy(&child_hdp, true);
            }
            break;
        case EXT_NODE_AND_ATTR:
            complete = true;
            retb = false;
            break;
        default:
            retb = true;
            complete = true;
            DBUG_PRINT("ERROR", ("invalid extent value: %d", extent));
            break;
        }

        pag_rwlock_destroy(&dp->rwlock);
            
        free(dp);
        ndp = dp = NULL;
        *rows = node_deleted;
        
    }
    DBUG_RETURN(retb);
}

static bool find_ndp_func(void *item, void *dp)
{
    return(item == dp);
}

/* 
 * head_list_destroy - this destroys the head.
 *
 * where:
 *    @param0: <hdp>           -I
 *        pointer to the head list, this will be NULLed upon freeeing
 *    @param1  <force>        -I
 *        force = true if the list != NULL or num_elements != 0 it
 *        will ignore and free it. else it will do nothing.
 */
static bool head_list_destroy(head_list_p *hdp, bool force)
{
    static char *mod = "head_list_destroy";
    bool retb = false;

    DBUG_ENTER(mod);
    DBUG_PRINT("DEBUG", ("force=%d", force));
    pag_rwlock_wrlock(&(*hdp)->rwlock);
    head_change_state((*hdp), HD_ST_DESTROYING);
    if ((*hdp)->list && (*hdp)->num_elements) {
        DBUG_PRINT("WARN", ("The node list in not empty:%p:%d",
                  (*hdp)->list, (*hdp)->num_elements));
        if (force == false)
            goto rtn_return;
    }
    sequence_destroy(&(*hdp)->sequence);
    node_list_free_all(*hdp);
    free(*hdp);
    pag_rwlock_destroy(&(*hdp)->rwlock);        
    *hdp = NULL;
 rtn_return:    
    DBUG_RETURN(retb);
}

/*
 * node_list_insert - adds a node to the list, add the node to the 
 *    indexes.
 * 
 * where:
 * @param0 <hdp>         - I
 *        point to the head of the list.
 * @param1 <ndp>        - I
 * @returns <node_id>   - O
 * 
 */
uint32_t node_list_insert(head_list_p hdp, node_ele_p ndp)
{
    static char *mod = "node_list_insert";
    uint32_t rtn_id = 0;
    enum_head_state save_hd_state;

    DBUG_ENTER(mod);
    pag_rwlock_wrlock(&ndp->rwlock);
    pag_rwlock_wrlock(&hdp->rwlock);
    save_hd_state = hdp->state;
    ndp->state = ND_ST_PENDING_INSERT;
    rtn_id = ndp->id = sequence_next(&hdp->sequence);
    if (list_add(&hdp->list, -1, ndp)) {
        DBUG_PRINT("WARN", ("can't add node to the list."));
        rtn_id = 0;
        ndp->state = ND_ST_LIST_ADD_ERROR;
        hdp->state = save_hd_state;
        pag_rwlock_unlock(&hdp->rwlock);
        goto rtn_return;
    }
    hdp->num_elements++;
    hdp->state = save_hd_state;
    pag_rwlock_unlock(&hdp->rwlock);
    
    /* update the indexes */
    if (index_add_node_to_all(index_init_list, ndp)) {;
        DBUG_PRINT("ERROR", ("can't update indexes with node id:%d.", ndp->id));
        ndp->state = ND_ST_INDEX_ERROR;
        rtn_id = 0;
    }

    ndp->state = ND_ST_PENDING_ENFORCEMENT;
    ndp->enforce_state = EF_ST_PENDING;
 rtn_return:
    hdp->state = HD_ST_CALM;
    pag_rwlock_unlock(&ndp->rwlock);
    DBUG_RETURN(rtn_id);
}

/*
* node_list_free_all - this will free everything in the Node list
*    including the attributes assciated to each node.
*
* where:
*
*/
static bool node_list_free_all(head_list_p hdp)
{
    static char *mod = "node_list_free_all";
    bool retb = false;
    node_ele_t *ndp;
    int save_hd_state;

    DBUG_ENTER(mod);
    pag_rwlock_wrlock(&hdp->rwlock);
    save_hd_state = hdp->state;
    hdp->state = HD_ST_FREEALL;
    
    while (hdp->num_elements) {
        ndp = list_delete(&hdp->list, 1);
        if (ndp) {
            node_ele_destroy(&ndp);
            ndp = NULL;
        }
        hdp->num_elements--;
    }

    hdp->state = save_hd_state;
    pag_rwlock_wrlock(&hdp->rwlock);
    DBUG_RETURN(retb);
}

/* node_ele_destroy - frees and node_ele_t and its attr.
 *
 * where:
 * @param0 <ndp>                   - I
 *         pointer to the ndp ele
 *
 */
static void node_ele_destroy(node_ele_p *ndp)
{
    static char *mod = "node-ele_destroy";
    DBUG_ENTER(mod);
    
    pag_rwlock_wrlock(&(*ndp)->rwlock);
    DBUG_PRINT("DEBUG", ("deleting %p:%d", *ndp, (*ndp)->uri->hash));
    
    /* free the propoerties */
    if ((*ndp)->properties_list->num_elements) {
        attr_list_free_all(&(*ndp)->properties_list);
    }
    free((*ndp)->lri);
    parsed_uri_free(&(*ndp)->uri);
    pag_rwlock_destroy(&(*ndp)->rwlock);
    free(*ndp);
    DBUG_VOID_RETURN;
}

/* dump_node - dumps the data in a node.
 *
 * where:
 * @param0 <ndp>          - I
 *         pointer to the node (node_ele_t *)
 *
 */
void dump_node(node_ele_p ndp)
{
    DBUG_PRINT("INFO", (" ****** Node[%p]: id=%d class_id:%d cpid:%d",
                        ndp, ndp->id, ndp->class_id, ndp->content_path_id));
    DBUG_PRINT("INFO", ("  state:%s enfstate:%s", node_state_msg[ndp->state],
                        enforcement_state_msg[ndp->enforce_state]));
    DBUG_PRINT("INFO", ("  lri:%s", ndp->lri));
    DBUG_PRINT("INFO", ("  uri:uri:%s", ndp->uri->uri));
    DBUG_PRINT("INFO", ("  uri:host:%s", ndp->uri->host));
    DBUG_PRINT("INFO", ("  uri:hash:%d", ndp->uri->hash));
    DBUG_PRINT("INFO", ("  uri:path:%s", ndp->uri->path));
    DBUG_PRINT("INFO", ("  parent:%p", ndp->parent));
    DBUG_PRINT("INFO", ("  uri:%p", ndp->child_list));
    DBUG_PRINT("INFO", ("  uri:%p", ndp->properties_list));
}


/***************************************************************************
 * ATTRIBUTE SUPPORT CODE.
 **************************************************************************/

/*
 * attr_list_free_all - runs the attr list and frees all.
 *   Will free the head as well and NULL it. 
 *
 * where:
 * @param0 <ahdp>      - I
 *    attr head pointer
 *
 */
static bool attr_list_free_all(head_list_p *ahdp)
{
    static char *mod = "attr_list_free_all";
    bool retb = false;
    atrribute_t *attr_p = NULL;

    DBUG_ENTER(mod);
    if ((*ahdp)) {
        pag_rwlock_wrlock(&(*ahdp)->rwlock);
        while ((*ahdp)->num_elements) {
            attr_p = list_delete(&(*ahdp)->list, 1);
            /* Note: the index is deleted from node_delete operation */        
            attr_ele_destroy(&attr_p);
        }
        pag_rwlock_destroy(&(*ahdp)->rwlock);
        free(*ahdp);
        *ahdp = NULL;
    }
    DBUG_RETURN(retb);
}

/* 
 * attr_free_node - this understands the details of the attr and 
 *  frees the allocated memory of it element.
 *
 * where:
 * @param0 <attrp>            - I
 *         attribute_p
 *
 */
static void attr_ele_destroy(attribute_p *attrp)
{
    static char *mod = "attr_ele_destory";

    DBUG_ENTER(mod);
    free((*attrp)->field_name);
    free((*attrp)->dp);
    free(*attrp);
    DBUG_VOID_RETURN;
}

/*
 * attr_add_to_node - add an attribute to a node.
 *
 * where:
 * @param0: <np>           - I
 *          this is the node ptr to where the propoerty is to be added.
 * @param1: <attrp>        - I
 *          the attribute to be addes.
 * @returns: 0 sucess, else 1.
 *
 */
static bool attr_add_to_node(node_ele_p ndp, attribute_p attrp)
{
    static char *mod = "attr_add_to_node";
    bool retb = false;

    DBUG_ENTER(mod);
    pag_rwlock_wrlock(&ndp->rwlock);
    list_add(&ndp->properties_list->list, 1, (void *)attrp);
    ndp->properties_list->num_elements++;
    pag_rwlock_unlock(&ndp->rwlock);
    DBUG_RETURN(retb);
}

/***************************************************************************
 * INDEX SUPPORT CODE.
 **************************************************************************/

/*
 * index_create - create a hash index table & initialize.
 * 
 * where:
 * param0 <hip>         - I
 *        point hash_index structure to init
 * parap1 <name>        - I
 *        name to be assigned to the index
 * returns <retb>       - O
 *        0, will error out if it falis.
 */
static bool index_create(hash_index_p hip, char *name)
{
    static char *mod = "index_create";
    bool retb = false;

    DBUG_ENTER(mod);
    hip->name = strdup(name);
    shash_init(&hip->htable);
    pag_rwlock_init(&hip->rwlock);
    DBUG_RETURN(retb);
}

/* index_get_ptr - retrives a single pointer to a node_ele_p from the 
 * index defined by itype. If the key is not found in index a NULL is returned.
 *
 * where:
 *  @param0 <idp>                          - I
 *          an array of items that will be used as a key.
 *  @param2 <itype>                        - I
 *          which index to use.
 * @returns <ndp>                   
 *          node_ele_p.
 */
static node_ele_p index_get_ptr(hash_init_p hash_initp, void *idp, int itype)
{
    static char *mod = "index_get_ptrs";
    node_ele_p ndp = NULL;
    char key[64];
    hash_index_t *idxp = NULL;
    struct shash_node *shp;

    DBUG_ENTER(mod);

    /* so which index to use */
    idxp = (hash_initp+itype)->hidx;
    /* if its using an ID they have to be converted to string */
    if (itype == IT_URI_INDEX) {
        strcpy(key, idp);
        
    } else {
        sprintf(key, "%u", *(unsigned int *)idp);
    }
    pag_rwlock_rdlock(&idxp->rwlock);
    shp = shash_find(&idxp->htable, key);
    pag_rwlock_unlock(&idxp->rwlock);
    ndp = (node_ele_p)shp->data;

    DBUG_RETURN(ndp);
}

/*
 * index_destroy - destroy a hash_index table & initialize.
 * 
 * where:
 * param0 <hip>         - I
 *        point hash_index structure to init
 * parap1 <name>        - I
 *        name to be assigned to the index
 * returns <retb>       - O
 *        0, will error out if it falis.
 */
static bool index_destroy(hash_index_p hip)
{
    static char *mod = "index_destroy";
    bool retb = false;

    DBUG_ENTER(mod);
    pag_rwlock_wrlock(&hip->rwlock);
    shash_destroy(&hip->htable);
    free(hip->name);
    pag_rwlock_destroy(&hip->rwlock);
    DBUG_RETURN(retb);
}

/* 
 * index_add_node_to_all = used the various items in the node
 *  and adds it to all indexes.
 *
 * where:
 * @param0 <hash_init_p>          - I
 *         an arrary of all the hash indexes and there names.
 * @param1 <ndp>                 - I
 *         point to the node to be added.
 * @returns <retb>               - O
 *          0 sucess, else failure.
 *
 */
static bool index_add_node_to_all(hash_init_p hash_initp, node_ele_p ndp)
{
    static char *mod = "index_add_node_to_all";
    hash_init_p iilp = NULL;
    bool retb = false;
    char key[256];
    
    DBUG_ENTER(mod);

    for (iilp = hash_initp; iilp->name != NULL; iilp++) {
        memset(key, 0, sizeof(key));
        if (!strcmp(NODE_INDEX_NAME, iilp->name)) {
            sprintf(key, "%d", ndp->id);
        } 
        else if (!strcmp(CLASS_INDEX_NAME, iilp->name)) { 
            sprintf(key, "%d", ndp->class_id);
        }
        else if (!strcmp(URI_INDEX_NAME, iilp->name)) {
            memcpy(key, ndp->uri->uri, sizeof(key));
        }
        else {
            /* TODO dkehn: need to do the uri:attr key */
            memcpy(key, ndp->uri, sizeof(key));
        }
        index_add_node(iilp->hidx, ndp, key);
    }
    
    DBUG_RETURN(retb);
}

/* 
 * index_delete_node_from_all = delete e node from all the indexes.
 * Note: that the node's reference in the indexes is deleted ONLY,
 * it is ithe responsiblibilty of the caller to free the memory that
 * the node maintains, and all its other stuff (i.e. attr, etc.).
 *
 * where:
 * @param0 <hash_init_p>          - I
 *         an arrary of all the hash indexes and there names.
 * @param1 <ndp>                 - I
 *         point to the node to be deleted.
 * @returns <retb>               - O
 *          0 success, else failure.
 *
 */
static bool index_delete_node_from_all(hash_init_p hash_initp, node_ele_p ndp)
{
    static char *mod = "index_delete_node_from_all";
    hash_init_p iilp = NULL;
    struct shash_node *sndp;
    bool retb = false;
    char key[256];
    
    DBUG_ENTER(mod);

    for (iilp = hash_initp; iilp->name != NULL; iilp++) {
        memset(key, 0, sizeof(key));
        if (!strcmp(CLASS_INDEX_NAME, iilp->name)) {
            sprintf(key, "%d", ndp->class_id);
        } 
        else if (!strcmp(NODE_INDEX_NAME, iilp->name)) {
            sprintf(key, "%d", ndp->id);
        }
        else if (!strcmp(URI_INDEX_NAME, iilp->name)) {
            memcpy(key, ndp->uri->uri, sizeof(key));
        }
        else {
            /* TODO dkehn: need to do the uri:attr key */
            memcpy(key, ndp->uri, sizeof(key));
        }
        pag_rwlock_wrlock(&iilp->hidx->rwlock);
        sndp = shash_find((const struct shash *)&iilp->hidx->htable, key);
        if (sndp) {
            shash_delete(&iilp->hidx->htable, sndp);
        } else {
            DBUG_PRINT("ERROR", ("key:%s not found in %s index", 
                                 key, iilp->name));
        }
        pag_rwlock_unlock(&iilp->hidx->rwlock);
    }
    
    DBUG_RETURN(retb);
}

/* 
 * index_add_node - used the various items in the node
 *
 * where:
 * @param0 <ndp>                 - I
 *         point to the node to be added.
 * @returns <retb>               - O
 *          0 sucess, else failure.
 *
 */
static bool index_add_node(hash_index_p hip, node_ele_p ndp, char *key)
{
    static char *mod = "index_add_node";
    bool retb = false;
    
    DBUG_ENTER(mod);
    DBUG_PRINT("DEBUG", ("%s:%s", hip->name, key));
    pag_rwlock_wrlock(&hip->rwlock);
    shash_add(&hip->htable, key, ndp);
    pag_rwlock_unlock(&hip->rwlock);
    DBUG_RETURN(retb);
}

/* 
 * index_add_node - used the various items in the node
 *
 * where:
 * @param0 <ndp>                 - I
 *         point to the node to be added.
 * @returns <retb>               - O
 *          0 sucess, else failure.
 *
 */
static void index_dump(hash_init_p iilp, bool index_node_dump)
{
    static char *mod = "index_dump";
    hash_index_p hip;
    struct shash_node *snp;
    size_t count;
    
    DBUG_ENTER(mod);

    /* dump the indexes */
    DBUG_PRINT("INFO", ("*************************************"));
    DBUG_PRINT("INFO", ("***** INDEX DUMP ********************"));
    DBUG_PRINT("INFO", ("*************************************"));
    /* remove the indexes */
    for ( ;iilp->name != NULL; iilp++) {
        hip = iilp->hidx;
        pag_rwlock_rdlock(&hip->rwlock);
        count = shash_count(&hip->htable);
        DBUG_PRINT("INFO", (" IndexName:%s cnt:%d addr:%p", iilp->name, 
                            (int)count, (void *)&hip->htable));
        SHASH_FOR_EACH(snp, &hip->htable) {
            DBUG_PRINT("INFO", ("  name: %s data: %p", snp->name, snp->data));
            if (index_node_dump) {
                dump_node((node_ele_p)snp->data);
            }
        }
        
        pag_rwlock_unlock(&hip->rwlock);
    }

    DBUG_VOID_RETURN;
}
