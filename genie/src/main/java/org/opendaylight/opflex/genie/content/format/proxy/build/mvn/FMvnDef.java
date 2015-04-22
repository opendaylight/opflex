/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
package org.opendaylight.opflex.genie.content.format.proxy.build.mvn;

import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.*;
import org.opendaylight.opflex.genie.engine.proc.Config;

public class FMvnDef extends GenericFormatterTask
{
    public FMvnDef(
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
              null,
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
        return new FileNameRule(
                aInFnr.getRelativePath(),
                null,
                aInFnr.getFilePrefix(),
                aInFnr.getFileSuffix(),
                aInFnr.getFileExtension(),
                "pom");
    }

    public void generate()
    {
        generate(0, Config.getLibName());
    }

    public void generate(int aInIndent, String aInLibName)
    {
        out.println(aInIndent, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
        out.println(aInIndent, "<project xmlns=\"http://maven.apache.org/POM/4.0.0\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"http://maven.apache.org/POM/4.0.0 http://maven.apache.org/xsd/maven-4.0.0.xsd\">");
        out.println(aInIndent + 1, "<modelVersion>4.0.0</modelVersion>");
        out.println();
        out.println(aInIndent + 1, "<groupId>org.opendaylight.opflex</groupId>");
        out.println(aInIndent + 1, "<artifactId>" + aInLibName + "</artifactId>");
        out.println(aInIndent + 1, "<version>0.1.0-SNAPSHOT</version>");
        out.println(aInIndent + 1, "<packaging>jar</packaging>");
        out.println(aInIndent + 1, "<name>libopflex-" + aInLibName + "</name>");
        out.println(aInIndent + 1, "<description>genie generated " + aInLibName + " compatible with libopflex</description>");
        out.println(aInIndent + 1, "<url>https://wiki.opendaylight.org/view/OpFlex:Main</url>");
        out.println();
        out.println(aInIndent + 1, "<licenses>");
        out.println(aInIndent + 2, "<license>");
        out.println(aInIndent + 3, "<name>Eclipse Public License v1.0</name>");
        out.println(aInIndent + 3, "<url>https://www.eclipse.org/legal/epl-v10.html</url>");
        out.println(aInIndent + 3, "<distribution>repo</distribution>");
        out.println(aInIndent + 2, "</license>");
        out.println(aInIndent + 1, "</licenses>");
        out.println();
        out.println(aInIndent + 1, "<properties>");
        out.println(aInIndent + 2, "<project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>");
        out.println(aInIndent + 1, "</properties>");
        out.println();
        out.println(aInIndent + 1, "<build>");
        out.println(aInIndent + 2, "<plugins>");
        out.println(aInIndent + 3, "<plugin>");
        out.println(aInIndent + 4, "<groupId>org.apache.maven.plugins</groupId>");
        out.println(aInIndent + 4, "<artifactId>maven-compiler-plugin</artifactId>");
        out.println(aInIndent + 4, "<version>3.1</version>");
        out.println(aInIndent + 4, "<configuration>");
        out.println(aInIndent + 5, "<source>1.7</source>");
        out.println(aInIndent + 5, "<target>1.7</target>");
        out.println(aInIndent + 5, "<showDeprecation>true</showDeprecation>");
        out.println(aInIndent + 4, "</configuration>");
        out.println(aInIndent + 3, "</plugin>");
        out.println(aInIndent + 3, "<plugin>");
        out.println(aInIndent + 4, "<groupId>org.apache.maven.plugins</groupId>");
        out.println(aInIndent + 4, "<artifactId>maven-jar-plugin</artifactId>");
        out.println(aInIndent + 4, "<version>2.6</version>");
        out.println(aInIndent + 3, "</plugin>");
        out.println(aInIndent + 2, "</plugins>");
        out.println(aInIndent + 1, "</build>");
        out.println("</project>");
    }
}
