package org.opendaylight.opflex.genie.content.format.agent.build.automake.cpp;

import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.*;

/**
 * Created by midvorki on 10/7/14.
 */
public class FAutogenSH
        extends GenericFormatterTask
{
    public FAutogenSH(
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

    /**
     * Optional API required by the framework for transformation of file naming rule for the corresponding
     * generated file. This method can customize the location for the generated file.
     * @param aInFnr file name rule template
     * @return transformed file name rule
     */
    public static FileNameRule transformFileNameRule(FileNameRule aInFnr)
    {
        FileNameRule lFnr = new FileNameRule(
                aInFnr.getRelativePath(),
                null,
                aInFnr.getFilePrefix(),
                aInFnr.getFileSuffix(),
                aInFnr.getFileExtension(),
                "autogen");

        return lFnr;
    }


    public void firstLineCb()
    {
        out.println("#!/bin/sh");
    }


    public void generate()
    {
        out.println(FORMAT);
    }

    public static final String FORMAT = "# libopflex: a framework for developing opflex-based policy agents\n"
                                        + "# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.\n"
                                        + "#\n"
                                        + "# This program and the accompanying materials are made available under the\n"
                                        + "# terms of the Eclipse Public License v1.0 which accompanies this distribution,\n"
                                        + "# and is available at http://www.eclipse.org/legal/epl-v10.html\n" + "#\n"
                                        + "###########\n" + "#\n"
                                        + "# This autogen script will run the autotools to generate the build\n"
                                        + "# system.  You should run this script in order to initialize a build\n"
                                        + "# immediately following a checkout.\n" + "\n"
                                        + "for LIBTOOLIZE in libtoolize glibtoolize nope; do\n"
                                        + "    $LIBTOOLIZE --version 2>&1 > /dev/null && break\n" + "done\n" + "\n"
                                        + "if [ \"x$LIBTOOLIZE\" = \"xnope\" ]; then\n" + "    echo\n"
                                        + "    echo \"You must have libtool installed to compile this package.\"\n"
                                        + "    echo \"Download the appropriate package for your distribution,\"\n"
                                        + "    echo \"or get the source tarball at ftp://ftp.gnu.org/pub/gnu/\"\n"
                                        + "    exit 1\n" + "fi\n" + "\n" + "ACLOCALDIRS= \n"
                                        + "if [ -d  /usr/share/aclocal ]; then\n"
                                        + "    ACLOCALDIRS=\"-I /usr/share/aclocal\"\n" + "fi\n"
                                        + "if [ -d  /usr/local/share/aclocal ]; then\n"
                                        + "    ACLOCALDIRS=\"$ACLOCALDIRS -I /usr/local/share/aclocal\"\n" + "fi\n"
                                        + "\n" + "$LIBTOOLIZE --automake --force && \\\n"
                                        + "aclocal --force $ACLOCALDIRS && \\\n"
                                        + "autoconf --force $ACLOCALDIRS && \\\n" + "autoheader --force && \\\n"
                                        + "automake --add-missing --foreign && \\\n"
                                        + "echo \"You may now configure the software by running ./configure\" && \\\n"
                                        + "echo \"Run ./configure --help to get information on the options \" && \\\n"
                                        + "echo \"that are available.\"";


}
