/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflex/logging/internal/logging.hpp>
#include <stdlib.h>
#include <execinfo.h>
#include <cxxabi.h>

extern "C" void __cyg_profile_func_enter(void *this_fn, void *call_site)
                                        __attribute__((no_instrument_function));
extern "C" void __cyg_profile_func_exit(void *this_fn, void *call_site)
                                        __attribute__((no_instrument_function));

namespace opflex { namespace logging {
    class Instruction;

std::ostream& operator<< (std::ostream& os, Instruction * const func)
                                        __attribute__((no_instrument_function));

std::ostream& operator<< (std::ostream& os, Instruction * const func) {

    /* not thread-safe, but for now we don't care */
    static size_t funcnamesize = 256;
    static char * funcname = (char*)malloc(funcnamesize);

    // resolve addresses into strings containing "filename(function+address)",
    // this array must be free()-ed
    char** symbollist = backtrace_symbols(reinterpret_cast< void * const * >(&func), 1);

	char *begin_name = 0, *begin_offset = 0, *end_offset = 0;

	// find parentheses and +address offset surrounding the mangled name:
	// ./module(function+0x15c) [0x8048a6d]
	for (char *p = *symbollist; *p; ++p) {
	    if (*p == '(')
            begin_name = p;
	    else if (*p == '+')
            begin_offset = p;
        else if (*p == ')' && begin_offset) {
            end_offset = p;
            break;
	    }
	}

	if (begin_name && begin_offset && end_offset && begin_name < begin_offset) {
	    *begin_name++ = '\0';
	    *begin_offset++ = '\0';
	    *end_offset = '\0';

	    // mangled name is now in [begin_name, begin_offset) and caller
	    // offset in [begin_offset, end_offset). now apply
	    // __cxa_demangle():

	    int status;
	    char* ret = abi::__cxa_demangle(begin_name,
					    funcname, &funcnamesize, &status);
	    if (status == 0) {
            funcname = ret; // use possibly realloc()-ed string
            os << *symbollist << ": " << funcname << "+" << begin_offset;
	    } else {
            // demangling failed. Output function name as a C function with
            // no arguments.
            os << *symbollist << ": " << begin_name << "+" << begin_offset;
        }
	}
	else
	{
	    // couldn't parse the line? print the whole line.
        os << *symbollist;
	}

    free(symbollist);

    return os;
}

}}

extern "C" void __cyg_profile_func_enter(void *func, void *caller) {
    LOG(TRACE)
        << " -> "
        << static_cast< opflex::logging::Instruction * >(func)
        << " <- "
        << static_cast< opflex::logging::Instruction * >(caller)
    ;
}

extern "C" void __cyg_profile_func_exit(void *func, void *caller) {
    LOG(TRACE)
        << " <- "
        << static_cast< opflex::logging::Instruction * >(func)
        << " -> "
        << static_cast< opflex::logging::Instruction * >(caller)
    ;
}
