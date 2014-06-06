/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

#include "uri-parser.h"
#include "dbug.h"

#ifdef NDEBUG
#define DEBUG(...)
#else
#define DEBUG(...) printf(__VA_ARGS__)
#endif
/*----------------------------------------------------------------------------
 * unittests.
 */
static void test_uri_parse(void **state)
{
    (void) state;
    /* these are from RFC 3986 */
    char *test_uri[] = {
        "ftp://ftp.is.co.za/rfc/rfc1808.txt",
        "http://www.ietf.org/rfc/rfc2396.txt",
        "ldap://[2001:db8::7]/c=GB?objectClass?one",
        "https://git.opendaylight.org/gerrit/#/c/7788/",
        "https://mail.google.com/mail/u/1/#inbox",
        "https://git.opendaylight.org/gerrit/gitweb?p=opflex.git;a=commit;h=HEAD",
        "http://www.pierobon.org/iis/review1.htm.html#one",
        "http://en.wikipedia.org/wiki/Uniform_Resource_Identifier#Naming.2C_addressing.2C_and_identifying_resources",
        NULL
    };
    int a;
    parsed_uri_p pp;
    /* DBUG_PUSH("d:t:i:L:n:P:T:0"); */

    for (a=0; test_uri[a] != NULL; a++) {
        assert_false(parse_uri(&pp, test_uri[a]));
        parsed_uri_free(&pp);
    }
}

/* ---------------------------------------------------------------------------
 * Test driver.
 */
int main(int argc, char *argv[])
{
    int retc; 
    int a;

    const UnitTest tests[] = {
        unit_test(test_uri_parse),
    };

    retc = run_tests(tests);

    return retc;
}
