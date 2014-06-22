/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#define USE_VLOG 1

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
#include "ovs-thread.h"
#include "seq-util.h"
#include "hash-util.h"
#include "tv-util.h"
#include "dirs.h"
#include "modb.h"
#include "eventq.h"
#include "vlog.h"
#include "dbug.h"


VLOG_DEFINE_THIS_MODULE(modb);


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
static bool modb_initialized = false;

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
    {MODB_SECTION, ATTR_IDX_SZ_TAG, "8000"},
    {MODB_SECTION, "mod_debug", "false"},
    {NULL, NULL, NULL}
};

/* Index lookups
*/
static hash_index_t class_id_shash = {0};
static hash_index_t node_id_shash = {0};
static hash_index_t uri_shash = {0};
static hash_index_t attr_shash = {0};

static hash_init_t index_init_list[] = { 
    {NODE_INDEX_NAME, &node_id_shash},
    {CLASS_INDEX_NAME, &class_id_shash},
    {URI_INDEX_NAME, &uri_shash},
    {ATTR_INDEX_NAME, &attr_shash},
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
static bool head_list_destroy(head_list_p *hp, bool force);
uint32_t node_list_insert(head_list_p hdp, node_ele_p ndp);
static bool node_list_free_all(head_list_p hp);
static bool find_ndp_func(void *item, void *dp);
static bool node_list_delete(head_list_p hdp, node_ele_p ndp, int extent, int *rows);
static void node_ele_destroy(node_ele_p *ndp);
static const char *opcode_to_string(int opc);
static node_ele_p node_search(void *dp, enum_itype itype);

/* Attr protos */
static bool attr_list_free_all(head_list_p *ahdp);
static void attr_ele_destroy(attribute_p ap);
static bool attr_add_to_node(node_ele_p ndp, attribute_p attrp);
static bool attr_find_list_name(void *search_name, void *ap);
static char *attr_type_to_string(int ftype);

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
static int index_add_all_in_tree(node_ele_p parent);
static bool index_add_attr(hash_index_p hip, attribute_p ap);



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

    ENTER(mod);
#ifndef USE_VLOG
    DBUG_PROCESS("modb");
#endif

    if (!modb_initialized) {
        modb_tid = pthread_self();
        conf_initialize(modb_config_defaults);

        /* Initialize the indexes */
        for (iilp = index_init_list; iilp->name != NULL; iilp++) {
            index_create(iilp->hidx, iilp->name);
        }

        /* Create the node head.   */
        head_list_create(&node_list);

        /* create the sequence numbering for the head_list, this will be the 
         * unique node.ids for all nodes in the MODB */
        sequence_init(&modb_sequence, DEFAULT_SEQ_START_NUMBER,
                      DEFAULT_SEQ_INC);

        /* crash recovery? */
        if (strcasecmp(conf_get_value(MODB_SECTION, "crash_recovery"), "true") == 0) {
            modb_fname = fnm_create(".dat", MODB_FNAME, ovs_dbdir(), NULL);
            modb_crash_recovery(fnm_path(modb_fname));
        }

        modb_initialized = true;
    }
    LEAVE(mod);
    return(retb);
}

/* modb_is_intiialized - returns the modb_initialized flag.
 */
bool modb_is_initialized(void) 
{
    return(modb_initialized);
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

    ENTER(mod);
    if (modb_is_initialized()) {
        ovs_rwlock_rdlock(&node_list->rwlock);
        state = node_list->state;
        ovs_rwlock_unlock(&node_list->rwlock);
    } else {
        state = HD_ST_NOT_INITIALIZED;
    }

    LEAVE(mod);
    return(state);
}

/* node_create - this wiil allocate the node_ele_t and initialize it
 * in preparation for inserting into the MODB.
 *
 * where:
 * @param0 <ndp>           - I/O
 *        This will be a pointer to a pointer , in otherwords this routine will
 *        allocate the memory and return it back via this ptr.
 * @param1 <uri>           - I 
 *        this is the point to the uri string that this node will be 
 *        know by, If NULL, the uri struct will not be allocated.
 * @param2 <lri>            - I
 *        If not NULL the lri will be loaded,
 * @param3 <ctid>           - I
 *        content_path_id.
 * @returns:
 *        0 if successful, else !=0
 * 
 */
bool node_create(node_ele_p *ndp, const char *uri_str, const char *lri, 
                 uint32_t ctid)
{
    static char *mod = "node_create";

#ifdef USE_VLOG
    VLOG_ENTER(mod);
    VLOG_DBG("%s: uri=%s lri=%s", mod, uri_str, lri);
#else
    DBUG_ENTER(mod);
    DBUG_PRINT("DEBUG",("uri=%s lri=%s", uri_str, lri));
#endif

    *ndp = xzalloc(sizeof(node_ele_t));
    ovs_rwlock_init(&(*ndp)->rwlock);
    (*ndp)->content_path_id = ctid;
    if (lri)
        (*ndp)->lri = strdup(lri);
    if (uri_str)
        parse_uri(&(*ndp)->uri, uri_str);
    (*ndp)->parent = NULL;
    (*ndp)->child_list = NULL;
    (*ndp)->properties_list = NULL;
    (*ndp)->state = ND_ST_PENDING_INSERT;

    LEAVE(mod);
    return(0);
}

/* node_delete - this wiil delete a node that is not in the DB.
 *
 * where:
 * @param0 <ndp>           - I
 *        The node pointer
 * @returns:
 *        0 if successful, else !=0
 * 
 */
bool node_delete(node_ele_p *ndp, int *del_cnt)
{
    static char *mod = "node_delete";
    bool retb = false;
    bool complete = false;
    int child_cnt;
    node_ele_p cndp;
    head_list_p child_hdp, attr_hdp;
    int rcnt;

    ENTER(mod);
#ifdef USE_VLOG
    VLOG_INFO("uri=%s lri=%s", (*ndp)->uri->uri, (*ndp)->lri);
#else
    DBUG_PRINT("DEBUG",("uri=%s lri=%s", (*ndp)->uri->uri,
                        (*ndp)->lri));
#endif
    *del_cnt = 0;

    /* make sure this is not in the MODB */
    if (modb_initialized) {
        if (shash_find(&uri_shash.htable, (*ndp)->uri->uri) != NULL) {
            VLOG_ERR("%s: node: %s is found in the MODB", mod, (*ndp)->uri->uri);
            retb = true;
            goto rtn_return;
        }
    }
    
    while (complete == false) {
        ovs_rwlock_wrlock(&(*ndp)->rwlock);
        (*ndp)->state = ND_ST_DELETE;
        child_hdp = (*ndp)->child_list;
        attr_hdp = (*ndp)->properties_list;
        attr_list_free_all(&attr_hdp);

        if (child_hdp == NULL) {
            complete = true;
        } else {
            /* delete all the children associated to this node. */
            child_cnt = list_length(child_hdp->list);
            if (child_cnt != child_hdp->num_elements) {
                VLOG_WARN("%s: list num_elemetsts: %d not equal to actual: %d, "
                          "changing chdp->num_elements",
                          mod, child_hdp->num_elements, child_cnt);
                child_hdp->num_elements = child_cnt;
            }
            while (list_length(child_hdp->list)) {
                cndp = list_delete(&child_hdp->list, 1);
                if (cndp) {
                    node_delete(&cndp, &rcnt);
                    *del_cnt += rcnt;
                } else {
                    VLOG_ERR("%s: list_get return NULL, count:%d",
                             mod, child_cnt);
                    break;
                }
            }
            head_list_destroy(&child_hdp, true);
            complete = true;
        }
        ovs_rwlock_destroy(&(*ndp)->rwlock);
            
        free(*ndp);
        *del_cnt += 1;
    }
    
 rtn_return:
    LEAVE(mod);
    return(retb);
}

/* node_attach_child - this wiil attach child to parent nodes.
 *
 * where:
 * @param0 <parent>           - I
 *        pointer to the parent node
 * @param1 <child>           - I 
 *        point to the child that will be attached to the child
 * @returns:
 *        0 if successful, else !=0
 * 
 */
bool node_attach_child(node_ele_p parent, node_ele_p child)
{
    static char *mod = "node_attach_child";

    ENTER(mod);

    ovs_rwlock_wrlock(&parent->rwlock);
    if (parent->child_list == NULL) {
        head_list_create(&parent->child_list);
        parent->child_list->state = HD_ST_LOADED;
    }
    ovs_rwlock_wrlock(&child->rwlock);
    child->parent = parent;
    list_add(&parent->child_list->list, -1, (void *)child);
    parent->child_list->num_elements++;
    ovs_rwlock_unlock(&child->rwlock);
    ovs_rwlock_unlock(&parent->rwlock);

    LEAVE(mod);
    return(0);
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

    ENTER(mod);
#ifdef USE_VLOG
    VLOG_INFO("op=%s",opcode_to_string(operation));
#else
    DBUG_PRINT("DEBUG", ("op=%s",opcode_to_string(operation)));
#endif

    memset(resultp, 0, sizeof(result_t));
    resultp->ret_code = OP_RC_SUCCESS;
    resultp->op = operation;
    memset(resultp->err_msg, 0, ERR_MSG_SZ);

    if (!modb_initialized) {
        sprintf(resultp->err_msg, "MODB not initialized");
        resultp->ret_code = OP_RC_ERROR;
        goto rtn_return;
    }
    switch (operation) {
    case OP_INSERT:
        for (i=0, ndp=(node_ele_p)dp; i < dp_count; ndp++, i++) {
            switch (extent) {
            case EXT_FULL:
            case EXT_NODE:
                id = node_list_insert(node_list, ndp);
                if (id == 0) {
                    resultp->ret_code = OP_RC_ERROR;
                    sprintf(resultp->err_msg,"%s: Error inserting node inserted:%d: outof:%d", 
                            mod, resultp->rows, dp_count);
                    VLOG_ERR(resultp->err_msg);
                    break;
                }
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
        VLOG_ERR(resultp->err_msg);
        break;
    }

 rtn_return:
    elapsed = tv_subtract(tv_tod(), start);
    memset(resultp->elapsed, 0, ELAPSED_TIME_SZ);
    sprintf(resultp->elapsed,"%s secs.", tv_show(elapsed, false, NULL));
    LEAVE(mod);
    return(resultp->ret_code);
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

/* extent_to_string - converts the operation code to string.
 */
static const char *extent_to_string(int opc)
{
    static char *opstring[] = {
        "NOP",
        "FULL",
        "NODE",
        "NODE_AND_ATTR",
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

    ENTER(mod);

    if (modb_initialized) {
        /* remove the indexes */
        for (iilp = index_init_list; iilp->name != NULL; iilp++) {
            index_destroy(iilp->hidx);
        }
        modb_initialized = false;
    }
    LEAVE(mod);
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

    ENTER(mod);
    if (modb_initialized) {
        ovs_rwlock_rdlock(&node_list->rwlock);
        count = (size_t)list_length(node_list->list);

        /* dump the head */
#ifdef USE_VLOG
        VLOG_INFO("*************************************");
        VLOG_INFO("***** MODB DUMP *********************");
        VLOG_INFO("*************************************");
        VLOG_INFO(" State:%s", head_state_msg[node_list->state]);
        VLOG_INFO(" head count:%d   actual:%d", node_list->num_elements,
                  (int)count);
#else
        DBUG_PRINT("INFO", ("*************************************"));
        DBUG_PRINT("INFO", ("***** MODB DUMP *********************"));
        DBUG_PRINT("INFO", ("*************************************"));
        DBUG_PRINT("INFO", (" State:%s", head_state_msg[node_list->state]));
        DBUG_PRINT("INFO", (" head count:%d   actual:%d", node_list->num_elements,
                            (int)count));
#endif
    
        /* Dump the node data */
        for (i=1; i <= count; i++) {
            ndp = list_get(node_list->list, i);
            if (ndp == NULL) {
                VLOG_ERR("%s: ndp is NULL: %d", mod, i);
                break;
            } else {
                node_dump(ndp);
            }
        }
        ovs_rwlock_unlock(&node_list->rwlock);

        index_dump(index_init_list, index_node_dump);
    }
    LEAVE(mod);
    return;
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
    
    ENTER(mod);
#ifdef USE_VLOG
    VLOG_INFO("dbfile=%s", dbfile);
#else
    DBUG_PRINT("DEBUG", ("dbfile=%s", dbfile));
#endif

    if (modb_initialized) {
        dbpath = (char *)ovs_dbdir();
        if (fnm_exists(dbfname)) {
            DBUG_PRINT("DEBUG" , ("%s: loading: %s\n", mod, fnm_path(dbfname)));

            /* TODO: dkehn, must build out the crash recovery */
            retb = 0;
        }
    }
    LEAVE(mod);
    return(retb);
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
bool head_list_create(head_list_p *hdp) 
{
    static char *mod = "head_list_create";
    bool retb = false;

    ENTER(mod);
    *hdp = xzalloc(sizeof(head_list_t));
    ovs_rwlock_init(&(*hdp)->rwlock);
    ovs_rwlock_wrlock(&(*hdp)->rwlock);
    (*hdp)->num_elements = 0;
    (*hdp)->list = NULL;
    head_change_state((*hdp), HD_ST_INITIALIZED);
    ovs_rwlock_unlock(&(*hdp)->rwlock);

    LEAVE(mod);
    return(retb);
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

    ENTER(mod);
    DBUG_PRINT("DEBUG", ("force=%d", force));
    ovs_rwlock_wrlock(&(*hdp)->rwlock);
    head_change_state((*hdp), HD_ST_DESTROYING);
    if ((*hdp)->list && (*hdp)->num_elements) {
        VLOG_WARN("The node list in not empty:%p:%d",
                  (*hdp)->list, (*hdp)->num_elements);
        if (force == false)
            goto rtn_return;
    }
    sequence_destroy(&modb_sequence);
    node_list_free_all(*hdp);
    free(*hdp);
    ovs_rwlock_destroy(&(*hdp)->rwlock);        
    *hdp = NULL;
 rtn_return:    
    LEAVE(mod);
    return(retb);
}

/* node_search - this is used to find a node in the indexes, will
 *   return a point of that node to the caller.
 *
 * where:
 * @param0 <dp>               - I
 *          data pointer, can be a uri, node.id, class_id
 * @param0 <itype>            - I
 *          enum_itype, which defines with index to use.
 * @return <ndp>              - I
 *        if successful, will return a valid ndp from the MODB.
 *         
 */
static node_ele_p node_search(void *dp, enum_itype itype)
{
    static char *mod = "node_search";
    node_ele_p rtn_ndp = NULL;
    char key[256] = {0};
    struct shash_node *shp = NULL;
    hash_index_p hashp;
    
    ENTER(mod);
    if (modb_initialized) {
        switch (itype) {
        case IT_NODE_INDEX:
            sprintf(key, "%u", *((uint32_t *)dp));
            hashp = &node_id_shash;
            break;
        case IT_CLASS_ID_INDEX:
            sprintf(key, "%u", *((uint32_t *)dp));
            hashp = &class_id_shash;
            break;
        case IT_ANY:
        case IT_NODE:
            sprintf(key, "%s", ((node_ele_p)dp)->uri->uri);
            hashp = &uri_shash;
            break;
        case IT_URI_INDEX:
            sprintf(key, "%s", (char *)dp);
            hashp = &uri_shash;
            break;
        default:
            hashp = NULL;
            break;
        }
        if (hashp) {
            shp = shash_find(&hashp->htable, (char *)&key);
            if (shp) {
                rtn_ndp = (node_ele_p)shp->data;
            }
        }
    }
    LEAVE(mod);
    return(rtn_ndp);
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
    int lpos;
    int node_deleted;
    int rcnt;
    int child_cnt;
    head_list_p child_hdp, attr_hdp;
    
    ENTER(mod);
#ifdef USE_VLOG
    VLOG_DBG("node.id:%d extent:%s", ndp->id, extent_to_string(extent));
#else
    DBUG_PRINT("DEBUG", ("node.id:%d extent:%s", ndp->id, extent_to_string(extent)));
#endif
    node_deleted = 0;
    while (complete == false) {
        ovs_rwlock_wrlock(&hdp->rwlock);
        lpos = list_find(hdp->list, ndp, find_ndp_func);
        if (lpos == 0) {
            VLOG_ERR("%s: node id:%d uri:%s, not found in list.",
                     mod, ndp->id, ndp->uri->uri);
            complete = true;
            retb = true;
            break;
        }
            
        dp = list_delete(&hdp->list, lpos);
        hdp->num_elements--;
        ovs_rwlock_unlock(&hdp->rwlock);

        node_deleted++;

        ovs_rwlock_wrlock(&dp->rwlock);
        dp->state = ND_ST_DELETE;
        child_hdp = dp->child_list;
        attr_hdp = dp->properties_list;
        attr_list_free_all(&attr_hdp);
        index_delete_node_from_all(index_init_list, dp);

        switch (extent) {
        case EXT_FULL:
            if (child_hdp == NULL) {
                complete = true;
            } else {
                /* delete all the children associated to this node. */
                child_cnt = list_length(child_hdp->list);
                if (child_cnt != child_hdp->num_elements) {
                    VLOG_WARN("%s: list num_elemetsts: %d not equal to actual: %d, "
                              "changing chdp->num_elements",
                              mod, child_hdp->num_elements, child_cnt);
                    child_hdp->num_elements = child_cnt;
                }
                for (; child_cnt != 0; child_cnt--) {
                    cndp = list_get(child_hdp->list, -1);
                    if (cndp) {
                        node_list_delete(child_hdp, cndp, extent, &rcnt);
                        node_deleted += rcnt;
                    } else {
                        VLOG_ERR("%s: list_get return NULL, count:%d",
                                 mod, child_cnt);
                        break;
                    }
                }
                head_list_destroy(&child_hdp, true);
                complete = true;
            }
            break;
        case EXT_NODE_AND_ATTR:
            complete = true;
            retb = false;
            break;
        default:
            retb = true;
            complete = true;
            VLOG_ERR("invalid extent value: %d", extent);
            break;
        }

        ovs_rwlock_destroy(&dp->rwlock);
            
        free(dp);
        ndp = dp = NULL;
        *rows = node_deleted;
        
    }
    LEAVE(mod);
    return(retb);
}

static bool find_ndp_func(void *item, void *dp)
{
    return(item == dp);
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

    ENTER(mod);
    ovs_rwlock_wrlock(&ndp->rwlock);
    ovs_rwlock_wrlock(&hdp->rwlock);
    save_hd_state = hdp->state;
    ndp->state = ND_ST_PENDING_INSERT;
    rtn_id = ndp->id = sequence_next(modb_sequence);
    if (list_add(&hdp->list, -1, ndp)) {
        VLOG_ERR("can't add node to the list.");
        rtn_id = 0;
        ndp->state = ND_ST_LIST_ADD_ERROR;
        hdp->state = save_hd_state;
        ovs_rwlock_unlock(&hdp->rwlock);
        ovs_rwlock_unlock(&ndp->rwlock);
    } else {
        hdp->num_elements++;
        hdp->state = save_hd_state;
        hdp->state = HD_ST_CALM;
        ovs_rwlock_unlock(&hdp->rwlock);
        
        /* update the indexes, the index has to assume the locking. */
        /* if (index_add_node_to_all(index_init_list, ndp)) {; */
        ndp->state = ND_ST_PENDING_ENFORCEMENT;
        ndp->enforce_state = EF_ST_PENDING;
        ovs_rwlock_unlock(&ndp->rwlock);
        if (index_add_all_in_tree(ndp) == 0) {;
            VLOG_ERR("can't update indexes with node id:%d.", ndp->id);
            ndp->state = ND_ST_INDEX_ERROR;
            rtn_id = 0;
        }
    }

    LEAVE(mod);
    return(rtn_id);
}

/*
* node_list_free_all - this will free everything in the Node list
*    including the attributes assciated to each node.
* NOTE: this does not lock the hdp!
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

    ENTER(mod);
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
    LEAVE(mod);
    return(retb);
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
    ENTER(mod);
    
    ovs_rwlock_wrlock(&(*ndp)->rwlock);
    DBUG_PRINT("DEBUG", ("deleting %p:%d", *ndp, (*ndp)->uri->hash));
    
    /* free the propoerties */
    if ((*ndp)->properties_list->num_elements) {
        attr_list_free_all(&(*ndp)->properties_list);
    }
    free((*ndp)->lri);
    parsed_uri_free(&(*ndp)->uri);
    ovs_rwlock_destroy(&(*ndp)->rwlock);
    free(*ndp);
    LEAVE(mod);
    return;
}

/* node_dump - dumps the data in a node.
 *
 * where:
 * @param0 <ndp>          - I
 *         pointer to the node (node_ele_t *)
 *
 */
void node_dump(node_ele_p ndp)
{
#ifdef USE_VLOG
    VLOG_INFO(" ****** Node[%p]: id=%d class_id:%d cpid:%d",
              ndp, ndp->id, ndp->class_id, ndp->content_path_id);
    VLOG_INFO("  state:%s enfstate:%s", node_state_msg[ndp->state],
              enforcement_state_msg[ndp->enforce_state]);
    VLOG_INFO("  lri:%s", ndp->lri);
    VLOG_INFO("  uri:uri:%s", ndp->uri->uri);
    VLOG_INFO("  uri:host:%s", ndp->uri->host);
    VLOG_INFO("  uri:hash:%u", ndp->uri->hash);
    VLOG_INFO("  uri:path:%s", ndp->uri->path);
    VLOG_INFO("  parent:%p", ndp->parent);
    VLOG_INFO("  child_list:%p", ndp->child_list);
    VLOG_INFO("  properties_list:%p", ndp->properties_list);
    attr_dump(ndp);
#else
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
#endif
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
    attribute_t *attr_p = NULL;

    ENTER(mod);
    if ((*ahdp)) {
        ovs_rwlock_wrlock(&(*ahdp)->rwlock);
        while ((*ahdp)->num_elements) {
            attr_p = list_delete(&(*ahdp)->list, 1);
            /* Note: the index is deleted from node_delete operation */        
            attr_ele_destroy(attr_p);
        }
        ovs_rwlock_destroy(&(*ahdp)->rwlock);
        free(*ahdp);
        *ahdp = NULL;
    }
    LEAVE(mod);
    return(retb);
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
static void attr_ele_destroy(attribute_p ap)
{
    static char *mod = "attr_ele_destory";

    ENTER(mod);
    free(ap->field_name);
    free(ap->dp);
    free(ap);
    LEAVE(mod);
    return;
}

/*
 * attr_create - creates a strribute that would be attached to a node_ele_t
 *    If the node_ele_p is not NULL it will attache to the node's 
 *    properties_list.
 *
 * where:
 * @param0: <ap>           - I/O
 *          this is a ** to where the attributes will be returned
 * @param1 <name>          - I
 *          the name of the attribute.
 * @param2 <atype>         - I
 *          enum_field_type for the dp.
 * @param3 <dp>            - I 
 *          pointere to the data.
 * @param4 <dp_len>        - I
 *          The len of data in dp should include room for ending '\0' 
 *          if necessary.
 * @param5 <ndp>           - I
 *          If present (non-NULL), the newly created ap will be 
 *          attached to this ndp.
 * @returns: 0 sucess, else 1.
 *
 */
bool attr_create(attribute_p *ap, char *name, int atype,
                 void *dp, uint32_t dp_len, node_ele_p ndp)
{
    static char *mod = "attr_create";
    bool retb = false;

    ENTER(mod);
    *ap = xzalloc(sizeof(attribute_t));
    (*ap)->ndp = NULL;
    /* (*ap)->field_name = xzalloc(strlen(name)+1); */
    /* memcpy((*ap)->field_name, name, strlen(name)); */
    (*ap)->field_name = strdup(name);
    (*ap)->dp = xzalloc(dp_len);
    memcpy((*ap)->dp, dp, dp_len);
    (*ap)->field_length = dp_len;
    (*ap)->attr_type = atype;
    (*ap)->flags = ATTR_FG_NEW;

    if (ndp) {
        attr_add_to_node(ndp, (*ap));
    }
    LEAVE(mod);
    return(retb);
}

/*
 * attr_delete - deletes an attribute associated to a node by its name, 
 *    if there are duplicates it will delete them all.
 *
 * where:
 * @param0: <ndp>           - I
 *          this is the node ptr to where the propoerty is to be added.
 * @param1: <name>        - I
 *          attribute name (field_name)
 * @returns: 0 success, else 1, NOTE: 1 is returned if not found, and
 *   0 returned if dups were found and deleted.
 *
 */
bool attr_delete(node_ele_p ndp, char *name)
{
    static char *mod = "attr_delete";
    bool retb = false;
    attribute_p del_attrp;
    int lpos;

    ENTER(mod);
    ovs_rwlock_wrlock(&ndp->rwlock);
    if (ndp->properties_list == NULL) {
        VLOG_ERR("%s: No property_list on node: %s", mod, ndp->uri->uri);
        retb = true;
        goto rtn_return;
    }

    while ( (lpos = list_find(ndp->properties_list->list, name, attr_find_list_name)) ) {
        del_attrp = list_delete(&ndp->properties_list->list, lpos);
        ndp->properties_list->num_elements--;
        attr_ele_destroy(del_attrp);
    }

 rtn_return:
    ovs_rwlock_unlock(&ndp->rwlock);
    LEAVE(mod);
    return(retb);
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
    node_ele_p sndp = NULL;
    attribute_p dup_attrp;
    int lpos;
    bool add_attr = true;

    ENTER(mod);
    ovs_rwlock_wrlock(&ndp->rwlock);
    attrp->ndp = ndp;
    if (ndp->properties_list == NULL) {
        head_list_create(&ndp->properties_list);
    }
    /* determine if this is a new or replace of an existing attr */
    lpos = list_find(ndp->properties_list->list, attrp->field_name, attr_find_list_name);
    if (lpos > 0) {
        dup_attrp = list_get(ndp->properties_list->list, lpos);
        if ( (dup_attrp->field_length == attrp->field_length) && 
             (!memcmp(dup_attrp->dp, attrp->dp, dup_attrp->field_length)) ) {
            attr_ele_destroy(attrp);
            add_attr = false;
        } else {
            dup_attrp->flags = ATTR_FG_OLD | ATTR_FG_DUP;
            attrp->flags |= ATTR_FG_DUP;
            add_attr = true;
        }
    }
        
    if (add_attr) {
        list_add(&ndp->properties_list->list, 1, (void *)attrp);
        ndp->properties_list->num_elements++;
        sndp = node_search(ndp->uri->uri, IT_URI_INDEX);
        if (sndp && sndp == ndp) {
            /* Index the attr the node is in the MODB */
            index_add_attr(&attr_shash, attrp);
        }
    }
    ovs_rwlock_unlock(&ndp->rwlock);

    LEAVE(mod);
    return(retb);
}

/* 
 * attr_find_list_name - list search function for the get_list call.
 * 
 * where:
 * @param0 <search_name>         - I
 *      the name we are looking for.
 * @param1 <ap>                  - I
 *      the attrp from the list to search against.
 * @returns <match>              - O
 *      false if no match, else true is does match
 */
static bool attr_find_list_name(void *search_name, void *ap) 
{
    static char *mod = "attr_find_list_name";
    bool match = false;
    
    ENTER(mod);
    if (!strcasecmp((char *)search_name, ((attribute_p)ap)->field_name)) {
        VLOG_DBG("%s: MATCH: s1:%s, s2:%s", mod, (char *) search_name, 
                 ((attribute_p)ap)->field_name);
        match = true;
    }
    LEAVE(mod);
    return(match);
}

/*
 * attr_dump - dumps the attr info
 *
 * where:
 * @param0 <ndp>       - I
 *
 */
void attr_dump(node_ele_p ndp)
{
    attribute_p ap;
    int act_count, i, lpos;

    if (ndp->properties_list) {
        act_count = list_length(ndp->properties_list->list);
        VLOG_INFO(" === Properties: count: %d actual:%d ===",
                  ndp->properties_list->num_elements, act_count);
        if (act_count != ndp->properties_list->num_elements) {
            ovs_rwlock_wrlock(&ndp->rwlock);
            ndp->properties_list->num_elements = act_count;
            ovs_rwlock_unlock(&ndp->rwlock);
        }
  
        for (i = 0, lpos = 1; i < act_count; i++, lpos++) {
            ap = list_get(ndp->properties_list->list, lpos);
            VLOG_INFO("  name:[%s] flags:0x%04x type:%s len:%d val:%s",
                      ap->field_name, ap->flags, 
                      attr_type_to_string(ap->attr_type), 
                      ap->field_length, (char *)ap->dp);
        }
    }
}

/*
 * attr_type_to_string - converts field type to string
 */
static char *attr_type_to_string(int ftype)
{
    static char *type_string[] = {
        "INTEGER",
        "LONG",
        "DATE",
        "STRING",
        "MAC",
        "STRING"
    };
    return(type_string[ftype]);
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

    ENTER(mod);
    hip->name = strdup(name);
    shash_init(&hip->htable);
    ovs_rwlock_init(&hip->rwlock);
    LEAVE(mod);
    return(retb);
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

    ENTER(mod);

    /* so which index to use */
    idxp = (hash_initp+itype)->hidx;
    /* if its using an ID they have to be converted to string */
    if (itype == IT_URI_INDEX) {
        strcpy(key, idp);
        
    } else {
        sprintf(key, "%u", *(unsigned int *)idp);
    }
    ovs_rwlock_rdlock(&idxp->rwlock);
    shp = shash_find(&idxp->htable, key);
    ovs_rwlock_unlock(&idxp->rwlock);
    ndp = (node_ele_p)shp->data;

    LEAVE(mod);
    return(ndp);
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

    ENTER(mod);
    ovs_rwlock_wrlock(&hip->rwlock);
    shash_destroy(&hip->htable);
    free(hip->name);
    ovs_rwlock_destroy(&hip->rwlock);
    LEAVE(mod);
    return(retb);
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
    
    ENTER(mod);

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
    LEAVE(mod);
    return(retb);
}

/*
 * index_add_all_in_tree - this will walk the tree and add all
 * childen ndp(s) to all indexes. 
 *
  * where:
 * param0 <ndp>         - I
 *        Points to the parent or branch of a tree that may or may not 
 *        have children associated to it.
 * retruns <ndp_count>      - O
 *        number of node_ele_t(s) encountered.
 */
static int index_add_all_in_tree(node_ele_p parent)
{
    static char *mod = "index_add_all_in_tree";
    head_list_p hdp;
    int head_cnt;
    node_ele_p ndp;
    int num_added = 0;
    int node_save_state;

    ENTER(mod);
    /* lock the parent */
    ovs_rwlock_rdlock(&parent->rwlock);
    node_save_state = parent->state;
    parent->state = ND_ST_INDEXING;
    index_add_node_to_all(index_init_list, parent);
    num_added++;
    if (parent->child_list) {
        hdp = parent->child_list;
        head_cnt = list_length(hdp->list);
        if (hdp->num_elements != head_cnt) {
            DBUG_PRINT("WARN", 
                       ("%s: list num_elemetsts: %d not euqal to actual: %d, changing nhead->num_elements",
                        mod, hdp->num_elements, head_cnt));
            hdp->num_elements = head_cnt;
        }
        for (; head_cnt != 0; head_cnt--) {
            ndp = (node_ele_p)list_get(hdp->list, head_cnt);
            /* if the unique ID has not been added, i.e. insert, add it. */
            ovs_rwlock_wrlock(&ndp->rwlock);
            if (ndp->id == 0) 
                ndp->id = sequence_next(modb_sequence);
            ndp->state = ND_ST_PENDING_ENFORCEMENT;
            ndp->enforce_state = EF_ST_NO_EVENT;
            ovs_rwlock_unlock(&ndp->rwlock);
            num_added += index_add_all_in_tree(ndp);
        }
    }
    parent->state = node_save_state;
    ovs_rwlock_unlock(&parent->rwlock);
    LEAVE(mod);
    return(num_added);
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
    
    ENTER(mod);

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
        ovs_rwlock_wrlock(&iilp->hidx->rwlock);
        sndp = shash_find((const struct shash *)&iilp->hidx->htable, key);
        if (sndp) {
            shash_delete(&iilp->hidx->htable, sndp);
        } else {
            VLOG_ERR("key:%s not found in %s index", key, iilp->name);
        }
        ovs_rwlock_unlock(&iilp->hidx->rwlock);
    }
    LEAVE(mod);
    return(retb);
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
    
    ENTER(mod);
    DBUG_PRINT("DEBUG", ("name:%s key:%s", hip->name, key));
    ovs_rwlock_wrlock(&hip->rwlock);
    shash_add(&hip->htable, key, ndp);
    ovs_rwlock_unlock(&hip->rwlock);
    LEAVE(mod);
    return(retb);
}

/* 
 * index_add_attr - add the attr to the attr_hash.
 *         uses the attribute.field_name as the key. Hence
 *         there should be a lot of dups.
 *
 * where:
 * @param0 <hip>                 - I
 *         hash_index_pointer
 * @param1 <ap>                  - I
 *         attribute_p to what is to be added.
 *
 */
static bool index_add_attr(hash_index_p hip, attribute_p ap)
{
    static char *mod = "index_add_attr";
    ENTER(mod);
    DBUG_PRINT("DEBUG", ("name:%s key:%s", hip->name, ap->field_name));
    ovs_rwlock_wrlock(&hip->rwlock);
    shash_add(&hip->htable, ap->field_name, ap);
    ovs_rwlock_unlock(&hip->rwlock);
    LEAVE(mod);
    return(0);
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
    
    ENTER(mod);

    /* dump the indexes */
#ifdef USE_VLOG
    VLOG_INFO("*************************************");
    VLOG_INFO("***** INDEX DUMP ********************");
    VLOG_INFO("*************************************");
#else
    DBUG_PRINT("INFO", ("*************************************"));
    DBUG_PRINT("INFO", ("***** INDEX DUMP ********************"));
    DBUG_PRINT("INFO", ("*************************************"));
#endif
    /* remove the indexes */
    for ( ;iilp->name != NULL; iilp++) {
        hip = iilp->hidx;
        ovs_rwlock_rdlock(&hip->rwlock);
        count = shash_count(&hip->htable);
#ifdef USE_VLOG
        VLOG_INFO(" ========= IndexName:%s cnt:%d addr:%p",
                  iilp->name, (int)count,
                  (void *)&hip->htable);
#else
        DBUG_PRINT("INFO", (" ========= IndexName:%s cnt:%d addr:%p",
                            iilp->name, (int)count,
                            (void *)&hip->htable));
#endif
        SHASH_FOR_EACH(snp, &hip->htable) {
#ifdef USE_VLOG
            VLOG_INFO("  name: %s data: %p", snp->name, snp->data);
#else
            DBUG_PRINT("INFO", ("  name: %s data: %p", snp->name, snp->data));
#endif
            if (index_node_dump) {
                node_dump((node_ele_p)snp->data);
            }
        }
        
        ovs_rwlock_unlock(&hip->rwlock);
    }
    LEAVE(mod);
}
