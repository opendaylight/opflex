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
        "mailto:John.Doe@example.com",
        "news:comp.infosystems.www.servers.unix",
        "tel:+1-816-555-1212",
        "telnet://192.0.2.16:80/",
        "urn:oasis:names:specification:docbook:dtd:xml:4.1.2",
        "http://www.pierobon.org/iis/review1.htm.html#one",
        "http://en.wikipedia.org/wiki/Uniform_Resource_Identifier#Naming.2C_addressing.2C_and_identifying_resources",
        NULL
    };
    int a;
    parsed_uri_t *pp;

    for (a=0; test_uri[a] != NULL; a++) {
        pp = parse_uri(test_uri[a]);
        if (pp == NULL)
            assert_null(pp);
        parsed_uri_free(pp);
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
