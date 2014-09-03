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

/*
 * Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013 Nicira, Inc.
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

#ifndef VLOG_OPFLEX_H
#define VLOG_OPFLEX_H 1

/* Logging.
 *
 *
 * Thread-safety
 * =============
 *
 * Fully thread safe.
 */

#include <stdarg.h>

#include "vlog.h"

#ifdef  __cplusplus
extern "C" {
#endif

#include "vlog.h"

#define FIN "<--"
#define FUNC_IN "<--"
#define FUNC_OUT "-->"
#define FOUT "-->"

#define VLOG_ENTER(a) VLOG_DBG("%s:%d:%s:<-", __FILE__, __LINE__, a)
#define VLOG_LEAVE(a) VLOG_DBG("%s:%d:%s:->", __FILE__, __LINE__, a)

#ifdef USE_VLOG
#define ENTER VLOG_ENTER
#define LEAVE(a) VLOG_LEAVE(a)
#endif

#ifdef  __cplusplus
}
#endif


#endif /* vlog-opflex.h */
