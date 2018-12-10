package org.opendaylight.opflex.genie.content.format.agent.build.automake.cpp;

import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.*;

public class FAxBoostSystemM4
        extends GenericFormatterTask
{
    public FAxBoostSystemM4(
            FormatterCtx aInFormatterCtx,
            FileNameRule aInFileNameRule,
            Indenter aInIndenter,
            BlockFormatDirective aInHeaderFormatDirective,
            BlockFormatDirective aInCommentFormatDirective,
            boolean aInIsUserFile,
            WriteStats aInStats)
    {
        super(aInFormatterCtx,
              aInFileNameRule,
              aInIndenter,
              aInHeaderFormatDirective,
              aInCommentFormatDirective,
              aInIsUserFile,
              aInStats);
    }

    public void generate()
    {
        out.println(FORMAT);
    }

    public static final String FORMAT = "# ===========================================================================\n"
                                        + "#     https://www.gnu.org/software/autoconf-archive/ax_boost_system.html\n"
                                        + "# ===========================================================================\n"
                                        + "#\n"
                                        + "# SYNOPSIS\n"
                                        + "#\n"
                                        + "#   AX_BOOST_SYSTEM\n"
                                        + "#\n"
                                        + "# DESCRIPTION\n"
                                        + "#\n"
                                        + "#   Test for System library from the Boost C++ libraries. The macro requires\n"
                                        + "#   a preceding call to AX_BOOST_BASE. Further documentation is available at\n"
                                        + "#   <http://randspringer.de/boost/index.html>.\n"
                                        + "#\n"
                                        + "#   This macro calls:\n"
                                        + "#\n"
                                        + "#     AC_SUBST(BOOST_SYSTEM_LIB)\n"
                                        + "#\n"
                                        + "#   And sets:\n"
                                        + "#\n"
                                        + "#     HAVE_BOOST_SYSTEM\n"
                                        + "#\n"
                                        + "# LICENSE\n"
                                        + "#\n"
                                        + "#   Copyright (c) 2008 Thomas Porschberg <thomas@randspringer.de>\n"
                                        + "#   Copyright (c) 2008 Michael Tindal\n"
                                        + "#   Copyright (c) 2008 Daniel Casimiro <dan.casimiro@gmail.com>\n"
                                        + "#\n"
                                        + "#   Copying and distribution of this file, with or without modification, are\n"
                                        + "#   permitted in any medium without royalty provided the copyright notice\n"
                                        + "#   and this notice are preserved. This file is offered as-is, without any\n"
                                        + "#   warranty.\n"
                                        + "\n"
                                        + "#serial 19\n"
                                        + "\n"
                                        + "AC_DEFUN([AX_BOOST_SYSTEM],\n"
                                        + "[\n"
                                        + "      AC_ARG_WITH([boost-system],\n"
                                        + " 	 AS_HELP_STRING([--with-boost-system@<:@=special-lib@:>@],\n"
                                        + "                    [use the System library from boost - it is possible to specify a certain library for the linker\n"
                                        + "                         e.g. --with-boost-system=boost_system-gcc-mt ]),\n"
                                        + "         [\n"
                                        + "         if test \"$withval\" = \"no\"; then\n"
                                        + " 			want_boost=\"no\"\n"
                                        + "         elif test \"$withval\" = \"yes\"; then\n"
                                        + "             want_boost=\"yes\"\n"
                                        + "             ax_boost_user_system_lib=\"\"\n"
                                        + "         else\n"
                                        + " 		    want_boost=\"yes\"\n"
                                        + " 		ax_boost_user_system_lib=\"$withval\"\n"
                                        + " 		fi\n"
                                        + "         ],\n"
                                        + "         [want_boost=\"yes\"]\n"
                                        + " 	)\n"
                                        + "\n"
                                        + " 	if test \"x$want_boost\" = \"xyes\"; then\n"
                                        + "         AC_REQUIRE([AC_PROG_CC])\n"
                                        + "         AC_REQUIRE([AC_CANONICAL_BUILD])\n"
                                        + " 		CPPFLAGS_SAVED=\"$CPPFLAGS\"\n"
                                        + " 		CPPFLAGS=\"$CPPFLAGS $BOOST_CPPFLAGS\"\n"
                                        + " 		export CPPFLAGS\n"
                                        + "\n"
                                        + " 		LDFLAGS_SAVED=\"$LDFLAGS\"\n"
                                        + " 		LDFLAGS=\"$LDFLAGS $BOOST_LDFLAGS\"\n"
                                        + " 		export LDFLAGS\n"
                                        + "\n"
                                        + "         AC_CACHE_CHECK(whether the Boost::System library is available,\n"
                                        + " 					   ax_cv_boost_system,\n"
                                        + "         [AC_LANG_PUSH([C++])\n"
                                        + " 			 CXXFLAGS_SAVE=$CXXFLAGS\n"
                                        + " 			 CXXFLAGS=\n"
                                        + "\n"
                                        + " 			 AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[@%:@include <boost/system/error_code.hpp>]],\n"
                                        + " 				    [[boost::system::error_category *a = 0;]])],\n"
                                        + "                    ax_cv_boost_system=yes, ax_cv_boost_system=no)\n"
                                        + " 			 CXXFLAGS=$CXXFLAGS_SAVE\n"
                                        + "              AC_LANG_POP([C++])\n"
                                        + " 		])\n"
                                        + " 		if test \"x$ax_cv_boost_system\" = \"xyes\"; then\n"
                                        + " 			AC_SUBST(BOOST_CPPFLAGS)\n"
                                        + "\n"
                                        + " 			AC_DEFINE(HAVE_BOOST_SYSTEM,,[define if the Boost::System library is available])\n"
                                        + "             BOOSTLIBDIR=`echo $BOOST_LDFLAGS | sed -e 's/@<:@^\\/@:>@*//'`\n"
                                        + "\n"
                                        + " 			LDFLAGS_SAVE=$LDFLAGS\n"
                                        + "             if test \"x$ax_boost_user_system_lib\" = \"x\"; then\n"
                                        + "                 for libextension in `ls -r $BOOSTLIBDIR/libboost_system* 2>/dev/null | sed 's,.*/lib,,' | sed 's,\\..*,,'` ; do\n"
                                        + "                      ax_lib=${libextension}\n"
                                        + " 				    AC_CHECK_LIB($ax_lib, exit,\n"
                                        + "                                  [BOOST_SYSTEM_LIB=\"-l$ax_lib\"; AC_SUBST(BOOST_SYSTEM_LIB) link_system=\"yes\"; break],\n"
                                        + "                                  [link_system=\"no\"])\n"
                                        + " 				done\n"
                                        + "                 if test \"x$link_system\" != \"xyes\"; then\n"
                                        + "                 for libextension in `ls -r $BOOSTLIBDIR/boost_system* 2>/dev/null | sed 's,.*/,,' | sed -e 's,\\..*,,'` ; do\n"
                                        + "                      ax_lib=${libextension}\n"
                                        + " 				    AC_CHECK_LIB($ax_lib, exit,\n"
                                        + "                                  [BOOST_SYSTEM_LIB=\"-l$ax_lib\"; AC_SUBST(BOOST_SYSTEM_LIB) link_system=\"yes\"; break],\n"
                                        + "                                  [link_system=\"no\"])\n"
                                        + " 				done\n"
                                        + "                 fi\n"
                                        + "\n"
                                        + "             else\n"
                                        + "                for ax_lib in $ax_boost_user_system_lib boost_system-$ax_boost_user_system_lib; do\n"
                                        + " 				      AC_CHECK_LIB($ax_lib, exit,\n"
                                        + "                                    [BOOST_SYSTEM_LIB=\"-l$ax_lib\"; AC_SUBST(BOOST_SYSTEM_LIB) link_system=\"yes\"; break],\n"
                                        + "                                    [link_system=\"no\"])\n"
                                        + "                   done\n"
                                        + "\n"
                                        + "             fi\n"
                                        + "             if test \"x$ax_lib\" = \"x\"; then\n"
                                        + "                 AC_MSG_ERROR(Could not find a version of the library!)\n"
                                        + "             fi\n"
                                        + " 			if test \"x$link_system\" = \"xno\"; then\n"
                                        + " 				AC_MSG_ERROR(Could not link against $ax_lib !)\n"
                                        + " 			fi\n"
                                        + " 		fi\n"
                                        + "\n"
                                        + " 		CPPFLAGS=\"$CPPFLAGS_SAVED\"\n"
                                        + " 	LDFLAGS=\"$LDFLAGS_SAVED\"\n"
                                        + " 	fi\n"
                                        + " ])\n";
}