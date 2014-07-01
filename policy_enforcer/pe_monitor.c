/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/

#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "misc-util.h"
#include "pe_monitor.h"
#include "command-line.h"
#include "column.h"
#include "compiler.h"
#include "daemon.h"
#include "dirs.h"
#include "dynamic-string.h"
#include "fatal-signal.h"
#include "json.h"
#include "jsonrpc.h"
#include "lib/table.h"
#include "ovsdb.h"
#include "ovsdb-data.h"
#include "ovsdb-error.h"
#include "poll-loop.h"
#include "sort.h"
#include "svec.h"
#include "stream.h"
#include "stream-ssl.h"
#include "ovsdb/table.h"
#include "timeval.h"
#include "unixctl.h"
#include "util.h"
#include "vlog.h"

VLOG_DEFINE_THIS_MODULE(pe_monitor);

static struct table_style table_style = TABLE_STYLE_DEFAULT;

/* protos */
static void
pe_check_ovsdb_error(struct ovsdb_error *error);
static void
pe_check_txn(int error, struct jsonrpc_msg **reply_);
static struct ovsdb_schema *
pe_fetch_schema(struct jsonrpc *rpc, const char *database);
static struct jsonrpc *
pe_open_jsonrpc(const char *server);
static struct json *
pe_parse_monitor_columns(char *arg, const char *server, const char *database,
                      const struct ovsdb_table_schema *table,
                      struct ovsdb_column_set *columns);
static void
pe_add_column(const char *server, const struct ovsdb_column *column,
           struct ovsdb_column_set *columns, struct json *columns_json);
static void
pe_ovsdb_monitor_block(struct unixctl_conn *conn, int argc OVS_UNUSED,
                   const char *argv[] OVS_UNUSED, void *blocked_);
static void
pe_ovsdb_monitor_unblock(struct unixctl_conn *conn, int argc OVS_UNUSED,
                     const char *argv[] OVS_UNUSED, void *blocked_);
static void
pe_ovsdb_monitor_exit(struct unixctl_conn *conn, int argc OVS_UNUSED,
                  const char *argv[] OVS_UNUSED, void *exiting_);
static void
pe_ovsdb_monitor(void *);
static void pe_add_monitored_table(const char *server,
                                const char *database,
                                struct ovsdb_table_schema *table,
                                struct json *monitor_requests,
                                struct monitored_table **mts,
                                size_t *n_mts,
                                size_t *allocated_mts);

/* routines */

void pe_monitor_init() {
    static char *mod = "pe_monitor_init";
    pthread_t ovsdb_monitor;
    bool pe_monitor_quit = false;

    VLOG_ENTER(mod);

    /* Separate thread that runs ovsdb monitor ALL */
    pag_pthread_create(&ovsdb_monitor,NULL,pe_ovsdb_monitor,NULL);

    pag_pthread_join(ovsdb_monitor,(void **) NULL); 

    VLOG_LEAVE(mod);

}

void pe_ovsdb_monitor(void *arg) {
    static char *mod = "pe_ovsdb_monitor";
    int save_errno = 0;
    struct jsonrpc *rpc;
    const char *server;
    const char *table_name = "ALL"; // monitor everything
    struct unixctl_server *unixctl;
    struct ovsdb_schema *schema;
    struct ovsdb_table_schema *table;
    struct jsonrpc_msg *request;
    struct json *monitor, *monitor_requests, *request_id;
    bool exiting = false;
    bool blocked = false;
    struct monitored_table *mts;
    size_t n_mts, allocated_mts, table_count, icount;
    const struct shash_node **nodes;
    const char *database = PE_OVSDB_NAME;
    char *json_string = NULL;

    VLOG_ENTER(mod); 

    VLOG_DBG("Opening %s\n", PE_OVSDB_SOCK);

    rpc = pe_open_jsonrpc(PE_OVSDB_SOCK);
    server = jsonrpc_get_name(rpc);

    if((save_errno = unixctl_server_create(NULL, &unixctl)) != 0)
        ovs_fatal(save_errno, "failed to create unixctl server");

    unixctl_command_register("exit", "", 0, 0,
                             pe_ovsdb_monitor_exit, &exiting);
    unixctl_command_register("ovsdb-monitor/block", "", 0, 0,
                             pe_ovsdb_monitor_block, &blocked);
    unixctl_command_register("ovsdb-monitor/unblock", "", 0, 0,
                                 pe_ovsdb_monitor_unblock, &blocked);

    schema = pe_fetch_schema(rpc, database);

    monitor_requests = json_object_create();

    mts = NULL;
    n_mts = allocated_mts = 0;

    table_count = shash_count(&schema->tables);
    nodes = shash_sort(&schema->tables);

    for (icount = 0; icount < table_count; icount++) {
        struct ovsdb_table_schema *table = nodes[icount]->data;

        pe_add_monitored_table(server, database, table,
                            monitor_requests,
                            &mts, &n_mts, &allocated_mts);
    }
    free(nodes);

    monitor = json_array_create_3(json_string_create(PE_OVSDB_NAME),
                                  json_null_create(), monitor_requests);
    request = jsonrpc_create_request("monitor", monitor, NULL);
    request_id = json_clone(request->id);
    jsonrpc_send(rpc, request);

    for (;;) {
        unixctl_server_run(unixctl);
        while (!blocked) {
            struct jsonrpc_msg *msg;
            int error;

            error = jsonrpc_recv(rpc, &msg);
            if (error == EAGAIN) {
                break;
            } else if (error) {
                ovs_fatal(error, "%s: receive failed", server);
            }

            if (msg->type == JSONRPC_REQUEST && !strcmp(msg->method, "echo")) {
                jsonrpc_send(rpc, jsonrpc_create_reply(json_clone(msg->params),
                                                       msg->id));
            } else if (msg->type == JSONRPC_REPLY
                       && json_equal(msg->id, request_id)) {
                VLOG_INFO("Message received: %s\n",
                          (json_string = json_to_string(msg->result,
                                                       JSSF_PRETTY)));
                //TODO: this data will have to be checked and pushed up
                //      not just printed
            } else if (msg->type == JSONRPC_NOTIFY
                       && !strcmp(msg->method, "update")) {
                struct json *params = msg->params;
                if (params->type == JSON_ARRAY
                    && params->u.array.n == 2
                    && params->u.array.elems[0]->type == JSON_NULL) {
                    VLOG_INFO("Message received: %s\n",
                              (json_string =
                                json_to_string(params->u.array.elems[1],
                                               JSSF_PRETTY)));
                //TODO: this data will have to be checked and pushed up
                //      not just printed
                }
            }
            jsonrpc_msg_destroy(msg);
        }

        if (exiting) {
            break;
        }

        jsonrpc_run(rpc);
        jsonrpc_wait(rpc);
        if (!blocked) {
            jsonrpc_recv_wait(rpc);
        }
        unixctl_server_wait(unixctl);
        poll_block();
    }

    VLOG_LEAVE(mod);

    pthread_exit(NULL);
}

static void
pe_ovsdb_monitor_exit(struct unixctl_conn *conn, int argc OVS_UNUSED,
                  const char *argv[] OVS_UNUSED, void *exiting_)
{
    bool *exiting = exiting_;
    *exiting = true;
    unixctl_command_reply(conn, NULL);
}

static void
pe_ovsdb_monitor_block(struct unixctl_conn *conn, int argc OVS_UNUSED,
                   const char *argv[] OVS_UNUSED, void *blocked_)
{
    bool *blocked = blocked_;

    if (!*blocked) {
        *blocked = true;
        unixctl_command_reply(conn, NULL);
    } else {
        unixctl_command_reply(conn, "already blocking");
    }
}

static void
pe_ovsdb_monitor_unblock(struct unixctl_conn *conn, int argc OVS_UNUSED,
                     const char *argv[] OVS_UNUSED, void *blocked_)
{
    bool *blocked = blocked_;

    if (*blocked) {
        *blocked = false;
        unixctl_command_reply(conn, NULL);
    } else {
        unixctl_command_reply(conn, "already unblocked");
    }
}

pe_add_monitored_table(const char *server,
                    const char *database,
                    struct ovsdb_table_schema *table,
                    struct json *monitor_requests,
                    struct monitored_table **mts,
                    size_t *n_mts,
                    size_t *allocated_mts)
{
    struct json *monitor_request_array;
    struct monitored_table *mt;

    if (*n_mts >= *allocated_mts) {
        *mts = x2nrealloc(*mts, allocated_mts, sizeof **mts);
    }
    mt = &(*mts)[(*n_mts)++];
    mt->table = table;
    ovsdb_column_set_init(&mt->columns);

    monitor_request_array = json_array_create_empty();
        /* Allocate a writable empty string since pe_parse_monitor_columns()
         * is going to strtok() it and that's risky with literal "". */
        char empty[] = "";
        json_array_add(
            monitor_request_array,
            pe_parse_monitor_columns(empty, server, PE_OVSDB_NAME,
                                  table, &mt->columns));

    json_object_put(monitor_requests, table->name, monitor_request_array);
}

static struct json *
pe_parse_monitor_columns(char *arg, const char *server, const char *database,
                      const struct ovsdb_table_schema *table,
                      struct ovsdb_column_set *columns)
{
    bool initial, insert, delete, modify;
    struct json *mr, *columns_json;
    char *save_ptr = NULL;
    char *token;

    mr = json_object_create();
    columns_json = json_array_create_empty();
    json_object_put(mr, "columns", columns_json);

    initial = insert = delete = modify = true;
    for (token = strtok_r(arg, ",", &save_ptr); token != NULL;
         token = strtok_r(NULL, ",", &save_ptr)) {
        if (!strcmp(token, "!initial")) {
            initial = false;
        } else if (!strcmp(token, "!insert")) {
            insert = false;
        } else if (!strcmp(token, "!delete")) {
            delete = false;
        } else if (!strcmp(token, "!modify")) {
            modify = false;
        } else {
            const struct ovsdb_column *column;

            column = ovsdb_table_schema_get_column(table, token);
            if (!column) {
                ovs_fatal(0, "%s: table \"%s\" in %s does not have a "
                          "column named \"%s\"",
                          server, table->name, database, token);
            }
            pe_add_column(server, column, columns, columns_json);
        }
    }

    if (columns_json->u.array.n == 0) {
        const struct shash_node **nodes;
        size_t i, n;

        n = shash_count(&table->columns);
        nodes = shash_sort(&table->columns);
        for (i = 0; i < n; i++) {
            const struct ovsdb_column *column = nodes[i]->data;
            if (column->index != OVSDB_COL_UUID
                && column->index != OVSDB_COL_VERSION) {
                pe_add_column(server, column, columns, columns_json);
            }
        }
        free(nodes);

        pe_add_column(server, ovsdb_table_schema_get_column(table, "_version"),
                   columns, columns_json);
    }

    if (!initial || !insert || !delete || !modify) {
        struct json *select = json_object_create();
        json_object_put(select, "initial", json_boolean_create(initial));
        json_object_put(select, "insert", json_boolean_create(insert));
        json_object_put(select, "delete", json_boolean_create(delete));
        json_object_put(select, "modify", json_boolean_create(modify));
        json_object_put(mr, "select", select);
    }

    return mr;
}

static void
pe_add_column(const char *server, const struct ovsdb_column *column,
           struct ovsdb_column_set *columns, struct json *columns_json)
{
    if (ovsdb_column_set_contains(columns, column->index)) {
        ovs_fatal(0, "%s: column \"%s\" mentioned multiple times",
                  server, column->name);
    }
    ovsdb_column_set_add(columns, column);
    json_array_add(columns_json, json_string_create(column->name));
}

static struct jsonrpc *
pe_open_jsonrpc(const char *server)
{
    struct stream *stream;
    int error;

    VLOG_ENTER("pe_open_jsonrpc");

    error = stream_open_block(jsonrpc_stream_open(server, &stream,
                              DSCP_DEFAULT), &stream);
    VLOG_DBG("Got %i from stream_open_block\n",error);
    if (error == EAFNOSUPPORT) {
        struct pstream *pstream;

        error = jsonrpc_pstream_open(server, &pstream, DSCP_DEFAULT);
        VLOG_DBG("Got %i from jsonrpc_stream_open\n",error);
        if (error) {
            ovs_fatal(error, "failed to connect or listen to \"%s\"", server);
        }
        VLOG_INFO("%s: waiting for connection...", server);
        error = pstream_accept_block(pstream, &stream);
        VLOG_DBG("Got %i from jsonrpc_stream_open\n",error);
        if (error) {
            ovs_fatal(error, "failed to accept connection on \"%s\"", server);
        }

        pstream_close(pstream);
    } else if (error) {
        ovs_fatal(error, "failed to connect to \"%s\"", server);
    }

    VLOG_LEAVE("pe_open_jsonrpc");

    return jsonrpc_open(stream);
}

static struct ovsdb_schema *
pe_fetch_schema(struct jsonrpc *rpc, const char *database)
{
    struct jsonrpc_msg *request, *reply;
    struct ovsdb_schema *schema;

    request = jsonrpc_create_request("get_schema",
                                     json_array_create_1(
                                         json_string_create(database)),
                                     NULL);
    pe_check_txn(jsonrpc_transact_block(rpc, request, &reply), &reply);
    pe_check_ovsdb_error(ovsdb_schema_from_json(reply->result, &schema));
    jsonrpc_msg_destroy(reply);

    return schema;
}

static void
pe_check_txn(int error, struct jsonrpc_msg **reply_)
{
    struct jsonrpc_msg *reply = *reply_;

    if (error) {
        ovs_fatal(error, "transaction failed");
    }

    if (reply->error) {
        ovs_fatal(error, "transaction returned error: %s",
                  json_to_string(reply->error, table_style.json_flags));
    }
}

static void
pe_check_ovsdb_error(struct ovsdb_error *error)
{
    if (error) {
        ovs_fatal(0, "%s", ovsdb_error_to_string(error));
    }
}

