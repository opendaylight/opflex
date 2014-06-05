/*-----------------------------------------------------------------c
 * op-enforcer-provider.h
 *
 * May 2014, Abilash Menon
 *
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 *------------------------------------------------------------------
 */

#ifndef OP_ENFORCER_PROVIDER_H
#define OP_ENFORCER_PROVIDER_H 1



/*
 * An OpenFlex enforcer
 */
struct op_enforcer {
    char *type;                 /* Enforcer type */
    char *name;                 /* Enforcer name */

    /* Other fields */
    uint64_t identity;         /* Identity */
};

/*
 *  op_enforcer class structure, to be defined by each oplfex enforcer
 *  implementation.
 */
struct op_enforcer_class {

    /* Initializes provider */
    void (*init)(void);
   
    /* Initialize the op-enforcer members and set up initial state */
    int (*construct)(struct op_enforcer *op_enforcer);

    /* Un-initialize the op-enforcer members and all states */
    void (*destruct)(struct op_enforcer *op_enforcer);

    /* Entity create */
    void (*entity_add) (struct op_enforcer *op_enforcer,
			struct op_enf_entity  *entity);

    /* Entity delete */
    void (*entity_delete) (struct op_enforcer *op_enforcer,
			struct op_enf_entity  *entity);

    /* Get features */
    void (*get_features)(struct op_enforcer *op_enforcer,
			char * features);

    /* Policy create */
    void (*policy_create) (struct op_enforcer *op_enforcer,
			struct op_enf_policy  *policy);

    /* Policy delete */
    void (*policy_delete) (struct op_enforcer *op_enforcer,
			struct op_enf_policy  *policy);

    /* Policy update */
    void (*policy_update) (struct op_enforcer *op_enforcer,
			struct op_enf_policy  *policy);
};



#endif /* OP_ENFORCER_PROVIDER_H */


