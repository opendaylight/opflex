package org.opendaylight.opflex.genie.content.format.agent.build.automake.cpp;

import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.*;

public class FAxBoostAsioM4
        extends GenericFormatterTask
{
    public FAxBoostAsioM4(
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

    public static final String FORMAT =
            "# ===========================================================================\n" +
            "#      https://www.gnu.org/software/autoconf-archive/ax_boost_asio.html\n" +
            "# ===========================================================================\n" +
            "#\n" +
            "# SYNOPSIS\n" +
            "#\n" +
            "#   AX_BOOST_ASIO\n" +
            "#\n" +
            "# DESCRIPTION\n" +
            "#\n" +
            "#   Test for Asio library from the Boost C++ libraries. The macro requires a\n" +
            "#   preceding call to AX_BOOST_BASE. Further documentation is available at\n" +
            "#   <http://randspringer.de/boost/index.html>.\n" +
            "#\n" +
            "#   This macro calls:\n" +
            "#\n" +
            "#     AC_SUBST(BOOST_ASIO_LIB)\n" +
            "#\n" +
            "#   And sets:\n" +
            "#\n" +
            "#     HAVE_BOOST_ASIO\n" +
            "#\n" +
            "# LICENSE\n" +
            "#\n" +
            "#   Copyright (c) 2008 Thomas Porschberg <thomas@randspringer.de>\n" +
            "#   Copyright (c) 2008 Pete Greenwell <pete@mu.org>\n" +
            "#\n" +
            "#   Copying and distribution of this file, with or without modification, are\n" +
            "#   permitted in any medium without royalty provided the copyright notice\n" +
            "#   and this notice are preserved. This file is offered as-is, without any\n" +
            "#   warranty.\n" +
            "\n" +
            "#serial 17\n" +
            "\n" +
            "AC_DEFUN([AX_BOOST_ASIO],\n" +
            "[\n" +
            "\tAC_ARG_WITH([boost-asio],\n" +
            "\tAS_HELP_STRING([--with-boost-asio@<:@=special-lib@:>@],\n" +
            "                   [use the ASIO library from boost - it is possible to specify a certain library for the linker\n" +
            "                        e.g. --with-boost-asio=boost_system-gcc41-mt-1_34 ]),\n" +
            "        [\n" +
            "        if test \"$withval\" = \"no\"; then\n" +
            "\t\t\twant_boost=\"no\"\n" +
            "        elif test \"$withval\" = \"yes\"; then\n" +
            "            want_boost=\"yes\"\n" +
            "            ax_boost_user_asio_lib=\"\"\n" +
            "        else\n" +
            "\t\t    want_boost=\"yes\"\n" +
            "\t\tax_boost_user_asio_lib=\"$withval\"\n" +
            "\t\tfi\n" +
            "        ],\n" +
            "        [want_boost=\"yes\"]\n" +
            "\t)\n" +
            "\n" +
            "\tif test \"x$want_boost\" = \"xyes\"; then\n" +
            "        AC_REQUIRE([AC_PROG_CC])\n" +
            "\t\tCPPFLAGS_SAVED=\"$CPPFLAGS\"\n" +
            "\t\tCPPFLAGS=\"$CPPFLAGS $BOOST_CPPFLAGS\"\n" +
            "\t\texport CPPFLAGS\n" +
            "\n" +
            "\t\tLDFLAGS_SAVED=\"$LDFLAGS\"\n" +
            "\t\tLDFLAGS=\"$LDFLAGS $BOOST_LDFLAGS\"\n" +
            "\t\texport LDFLAGS\n" +
            "\n" +
            "        AC_CACHE_CHECK(whether the Boost::ASIO library is available,\n" +
            "\t\t\t\t\t   ax_cv_boost_asio,\n" +
            "        [AC_LANG_PUSH([C++])\n" +
            "\t\t AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[ @%:@include <boost/asio.hpp>\n" +
            "\t\t\t\t\t\t\t\t\t\t\t]],\n" +
            "                                  [[\n" +
            "\n" +
            "                                    boost::asio::io_service io;\n" +
            "                                    boost::system::error_code timer_result;\n" +
            "                                    boost::asio::deadline_timer t(io);\n" +
            "                                    t.cancel();\n" +
            "                                    io.run_one();\n" +
            "\t\t\t\t\t\t\t\t\treturn 0;\n" +
            "                                   ]])],\n" +
            "                             ax_cv_boost_asio=yes, ax_cv_boost_asio=no)\n" +
            "         AC_LANG_POP([C++])\n" +
            "\t\t])\n" +
            "\t\tif test \"x$ax_cv_boost_asio\" = \"xyes\"; then\n" +
            "\t\t\tAC_DEFINE(HAVE_BOOST_ASIO,,[define if the Boost::ASIO library is available])\n" +
            "\t\t\tBN=boost_system\n" +
            "\t\t\tBOOSTLIBDIR=`echo $BOOST_LDFLAGS | sed -e 's/@<:@^\\/@:>@*//'`\n" +
            "            if test \"x$ax_boost_user_asio_lib\" = \"x\"; then\n" +
            "\t\t\t\tfor ax_lib in `ls $BOOSTLIBDIR/libboost_system*.so* $BOOSTLIBDIR/libboost_system*.dylib* $BOOSTLIBDIR/libboost_system*.a* 2>/dev/null | sed 's,.*/,,' | sed -e 's;^lib\\(boost_system.*\\)\\.so.*$;\\1;' -e 's;^lib\\(boost_system.*\\)\\.dylib.*$;\\1;' -e 's;^lib\\(boost_system.*\\)\\.a.*$;\\1;' ` ; do\n" +
            "\t\t\t\t    AC_CHECK_LIB($ax_lib, main, [BOOST_ASIO_LIB=\"-l$ax_lib\" AC_SUBST(BOOST_ASIO_LIB) link_thread=\"yes\" break],\n" +
            "                                 [link_thread=\"no\"])\n" +
            "\t\t\t\tdone\n" +
            "            else\n" +
            "               for ax_lib in $ax_boost_user_asio_lib $BN-$ax_boost_user_asio_lib; do\n" +
            "\t\t\t\t      AC_CHECK_LIB($ax_lib, main,\n" +
            "                                   [BOOST_ASIO_LIB=\"-l$ax_lib\" AC_SUBST(BOOST_ASIO_LIB) link_asio=\"yes\" break],\n" +
            "                                   [link_asio=\"no\"])\n" +
            "                  done\n" +
            "\n" +
            "            fi\n" +
            "            if test \"x$ax_lib\" = \"x\"; then\n" +
            "                AC_MSG_ERROR(Could not find a version of the library!)\n" +
            "            fi\n" +
            "\t\t\tif test \"x$link_asio\" = \"xno\"; then\n" +
            "\t\t\t\tAC_MSG_ERROR(Could not link against $ax_lib !)\n" +
            "\t\t\tfi\n" +
            "\t\tfi\n" +
            "\n" +
            "\t\tCPPFLAGS=\"$CPPFLAGS_SAVED\"\n" +
            "\tLDFLAGS=\"$LDFLAGS_SAVED\"\n" +
            "\tfi\n" +
            "])";

}