/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/
/*
 * The test-pe_scan.c file contains functions that test the
 * peovs_scan.c module.
 *
 * History:
 *     25-July-2014 smann@noironetworks.com = created.
 *
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "ovs-thread.h"
#include "misc-util.h"
#include "peovs_scan.h"
#include "vlog.h"

#define USE_VLOG 1

//void *test_monitor_init(void *input);


VLOG_DEFINE_THIS_MODULE(test_peovs_scan);

void rt_peovs_scan(void **state) {
    (void) state;
    
    pe_scan_init();

}

int main(void) {
    const UnitTest tests[] = {
        unit_test(rt_peovs_scan),
    };
    return run_tests(tests);
}
