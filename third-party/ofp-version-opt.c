/* Copyright (c) 2014 Cisco Systems, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <config.h>
#include "dynamic-string.h"
#include "ofp-util.h"
#include "ofp-version-opt.h"
#include "pag-thread.h"

static uint32_t allowed_versions = 0;

uint32_t
get_allowed_ofp_versions(void)
{
    return allowed_versions ? allowed_versions : OFPUTIL_DEFAULT_VERSIONS;
}

void
set_allowed_ofp_versions(const char *string)
{
    assert_single_threaded();
    allowed_versions = ofputil_versions_from_string(string);
}

void
mask_allowed_ofp_versions(uint32_t bitmap)
{
    assert_single_threaded();
    allowed_versions &= bitmap;
}

void
ofp_version_usage(void)
{
    struct ds msg = DS_EMPTY_INITIALIZER;

    ofputil_format_version_bitmap_names(&msg, OFPUTIL_DEFAULT_VERSIONS);
    printf(
        "\nOpen Flow Version options:\n"
        "  -V, --version           display version information\n"
        "  -O, --protocols         set allowed Open Flow versions\n"
        "                          (default: %s)\n",
        ds_cstr(&msg));
    ds_destroy(&msg);
}
