/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef MODB_H
#define MODB_H 1

#include "config-file.h"
#include "pag-thread.h"
#include "shash.h"
#include "list-util.h"
#include "seq-util.h"
#include "uri-parser.h"

#define MODB_SECTION "modb"
#define MODB_FNAME "mobdb.dat"


/* The initializer for the INIT_JMP_TABLE, see pagentd.c 
 */
#define MODB_INITIALIZE {"modb_init", modb_initialize}

/* These are the tags in the config file so the code knows what to load
 * in mulitple places. 
 */
#define CLASS_IDX_SZ_TAG "class_index_size"
#define NODE_IDX_SZ_TAG "node_index_size"
#define URI_IDX_SZ_TAG "uri_index_size"
#define URI_ATTR_IDX_SZ_TAG "uri_attr_index_size"

#define CLASS_IDX_DBUG_TAG "class_index_debug"
#define NODE_IDX_DBUG_TAG "node_index_debug"
#define URI_IDX_DBUG_TAG "uri_index_debug"
#define URI_ATTR_IDX_DBUG_TAG "uri_attr_index_debug"

/* index names */
#define NODE_INDEX_NAME "node_index"
#define CLASS_INDEX_NAME "class_index"
#define URI_INDEX_NAME "uri_index"
#define URI_ATTR_INDEX_NAME "uri:att_index"

/* The Node definition
 * The node is the basis of the database each object translated to a node and
 * object's paramters are properties to the node.
 */

/* The head_list is just that the head of a list, it will point the 
 * beginning and the end of a list.
 * Note: "*" implies locking required.
 * 
 * +------------+    +-----------+     +-----------+
 * | *head_list |--->| list_node |<--->| list_node | . . . . .
 * +------------+    +-----------+     +-----------+
 *                        |                 |
 *                        V                 V
 *                  +------------+     +------------+
 *                  | *node_ele  |     | *node_ele  | . . . 
 *                  +------------+     +------------+
 *                        |                 . . . . . . . . .
 *                        V
 *                  +-----------+     +-----------+     +-----------+
 *                  | head_list |<--->| list_node |<--->| list_node | . .
 *                  +-----------+     +-----------+     +-----------+
 *                                         |                 |
 *                                         V                 V
 *                                    +-----------+     +-----------+
 *                                    | attribute |     | attribute | . .
 *                                    +-----------+     +-----------+
 */

typedef enum _enum_head_state {
    HD_ST_CHECKPOINT_STARTED=0,
    HD_ST_CHECKPOINT_COMPLETE,
    HD_ST_INITIALIZED,
    HD_ST_DESTROYING,
    HD_ST_FREEALL,
    HD_ST_INSERT,
    HD_ST_UPDATE,
    HD_ST_DELETE,
    HD_ST_CALM,
    HD_ST_LOADED
} enum_head_state;


/* 
 * sequencer for the nodes in the MODB. This sequencer will be used for all
 * node.ids in the systems, to ensure uniqueness. 
*/
sequencer_p          modb_sequence;

/* 
 * head list for the node parents.
 */
typedef struct _head_list {
    struct pag_rwlock    rwlock;          /* this intended to lock the whole list */
    int                  num_elements;    /* number of list_node      */
    enum_head_state      state;
    list_node_p          list;            /* the defines the begineeing and end   */
} head_list_t, *head_list_p;

static inline int head_change_state(head_list_p hp, enum_head_state state)
{
    hp->state = state;
    return (hp->state);
}


typedef enum _enum_node_state {
    ND_ST_UNINITIALIZED=0,
    ND_ST_PENDING_INSERT,
    ND_ST_PENDING_UPDATE,
    ND_ST_PENDING_EVENT,
    ND_ST_PENDING_ENFORCEMENT,
    ND_ST_ENFORNCEMENT_SENT,
    ND_ST_PENDING_TRIGGER,
    ND_ST_TRIGGER_SENT,
    ND_ST_UPTODATE,
    ND_ST_INDEXING,
    ND_ST_INDEX_ERROR,
    ND_ST_LIST_ADD_ERROR,
    ND_ST_DELETE,
    ND_ST_CORRUPT
} enum_node_state;

typedef enum _enum_enforcement_state {
    EF_ST_NO_EVENT=0,
    EF_ST_PENDING,
    EF_ST_COMPLETE,
    EF_ST_FAILURE,
    EF_ST_CHANGE,
    EF_ST_UPDATED,
} enum_enforcement_state;

/* This defines the type of index to use for a query, usually
 * relates to what's behind the void * in the query.
 */
typedef enum _enum_itype {
    IT_NONE = 0,                 /* full search                     */
    IT_NODE,                     /* its a node_ele_p                */
    IT_NODE_INDEX,               /* query through node_id_index     */
    IT_CLASS_ID_INDEX,           /* query through class_id_index    */
    IT_URI_INDEX,                /* use the udi_index               */
} enum_itype;

/* This tells the nature of the operation:
 * FULL: the operation will effect the node andits children
 * NODE: only effects the Node and nothing else, no attrs.
 * NODE_AND_ATTR; effiect only the Node and its attr children are untouched.
 */
typedef enum _enum_query_extent {
    EXT_FULL = 1,
    EXT_NODE,
    EXT_NODE_AND_ATTR,
} enum_query_extent;

/* DB operation codes for what is to be done */
typedef enum _enum_q_op_code {
    OP_NOP=0,
    OP_INSERT,
    OP_DELETE,
    OP_SELECT,
    OP_UPDATE,
} enum_q_op_code;

/* 
 * operation return codes from the modb
 */
typedef enum _enum_op_rtn_code {
    OP_RC_SUCCESS=0,
    OP_RC_ERROR=-256,
} enum_op_rtn_code;

/* 
 * node definition in the MODB 
*/
typedef struct node_ele {
    struct pag_rwlock      rwlock;
    uint32_t               id;
    uint32_t               class_id;
    uint32_t               content_path_id;
    uint32_t               update_cnt;
    enum_node_state        state;
    enum_enforcement_state enforce_state;
    char                   *lri;
    parsed_uri_p           uri;
    struct node_ele        *parent;
    head_list_t            *child_list;
    head_list_t            *properties_list;
} node_ele_t, *node_ele_p;

/* 
 * attributes (properties)
 */
typedef enum _field_types {
    INTEGER = 1,
    LONG,
    DATE,
    STRING,
    MAC,
    IPADDR
} field_types;

/* 
 * An attribute is essentually a property on a Node 
 * we are defining this as a field, which is what it is in relationship
 * to the node.
 */
typedef struct _attribute {
    uint32_t         id;
    char             *field_name;
    /* this defines how the void *dp is interepted */
    field_types      attr_type;
    /* for string-type fields, this is the maximum number of characters. */
    uint32_t         field_length; 
    void             *dp;
} atrribute_t, *attribute_p;


/* This is the results that are returned from the various DB calls i.e.
 * select, delete, update, insert, etc.
 */
#define ERR_MSG_SZ 256
#define ELAPSED_TIME_SZ 40
typedef struct _result {
    enum_q_op_code op;         /* indicates the operation                    */
    enum_op_rtn_code ret_code; /* ret code where 0 is sucess                 */
    char err_msg[ERR_MSG_SZ];  /* error message, only valid if ret_code != 0 */
    char elapsed[ELAPSED_TIME_SZ];
    int rows;                  /* number rows effected                       */
    int itype;                 /* defines how to enterpret dp.               */
    void *dp;                  /* point to whatever is returned.             */
} result_t, *result_p;


/* 
 * index structures
 */
typedef struct hash_index {
    char *name;
    struct pag_rwlock rwlock;
    struct shash htable;
} hash_index_t, *hash_index_p;

/* List of indexes, this of initialization and destruction */
typedef struct _hash_init {
    char *name;
    hash_index_p hidx;
} hash_init_t, *hash_init_p;


/* 
 * Protos
 */
extern bool modb_initialize(void);
extern int  modb_op(int operation, void *dp, int itype, int dp_count,
                    int extent, result_p resultp);
extern void modb_cleanup(void);
extern bool modb_crash_recovery(const char *dbfile);
extern void modb_dump(bool index_node_dump);
extern enum_head_state modb_get_state(void);
extern void dump_node(node_ele_p ndp);
extern bool head_list_create(head_list_p *hdp);

#endif /* MODB_H */
