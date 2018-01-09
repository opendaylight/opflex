/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* Shim to allow compiling PacketInHandler_test against OVS headers
   that cannot be included in C++ */

#include <openvswitch/ofp-actions.h>

#define CHECKE(a, b) if (a != b) return i;

int check_action_learn1(const struct ofpact* acts, size_t len) {
    int i = 0;
    const struct ofpact* a;
    OFPACT_FOR_EACH (a, acts, len) {
        if (i == 0) CHECKE(OFPACT_SET_FIELD, a->type);
        if (i == 1) CHECKE(OFPACT_SET_FIELD, a->type);
        if (i == 2) CHECKE(OFPACT_OUTPUT, a->type);
        if (i == 3) CHECKE(OFPACT_OUTPUT, a->type);
        ++i;
    }
    if (i != 4) return 100+i;
    return -1;
}

int check_action_learn2(const struct ofpact* acts, size_t len) {
    int i = 0;
    const struct ofpact* a;
    OFPACT_FOR_EACH (a, acts, len) {
        if (i == 0) CHECKE(OFPACT_SET_FIELD, a->type);
        if (i == 1) CHECKE(OFPACT_GROUP, a->type);
        ++i;
    }
    if (i != 2) return 100+i;
    return -1;
}

int check_action_learn3(const struct ofpact* acts, size_t len) {
    int i = 0;
    const struct ofpact* a;
    OFPACT_FOR_EACH (a, acts, len) {
        if (i == 0) CHECKE(OFPACT_SET_FIELD, a->type);
        if (i == 1) CHECKE(OFPACT_SET_FIELD, a->type);
        if (i == 2) CHECKE(OFPACT_GROUP, a->type);
        ++i;
    }
    if (i != 3) return 100+i;
    return -1;
}

int check_action_learn4(const struct ofpact* acts, size_t len) {
    int i = 0;
    const struct ofpact* a;
    OFPACT_FOR_EACH (a, acts, len) {
        if (i == 0) CHECKE(OFPACT_SET_FIELD, a->type);
        if (i == 1) CHECKE(OFPACT_SET_FIELD, a->type);
        if (i == 2) CHECKE(OFPACT_GOTO_TABLE, a->type);
        ++i;
    }
    if (i != 3) return 100+i;
    return -1;
}
