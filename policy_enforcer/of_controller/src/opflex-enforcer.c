/*------------------------------------------------------------------
 * oplfex-enforcer.c
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
#include <config.h>

#include <stdio.h>

int ovs_ofctl_main(int argc, char *argv[]);

/*
 * main process 
 */
int
main (int argc, char *argv[])
{
    ovs_ofctl_main(argc, argv);

    printf("Dummy");
}

