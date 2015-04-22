package org.opendaylight.opflex.genie.engine.format;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/24/14.
 */
public enum FileTypeMeta
{
    C_HEADER("c:header",
             ".h",
             new Indenter("c-header", false, 10, 4, true),
             new BlockFormatDirective(
                    "/**",
                    " * ",
                    null,
                    " */",
                    0),
             new BlockFormatDirective(
                     "/*",
                     " * ",
                     null,
                     " */",
                     0)
             ),
    C_SOURCE("c:source",
             ".c",
             new Indenter("c-source", false, 10, 4, true),
             new BlockFormatDirective(
                     "/**",
                     " * ",
                     null,
                     " */",
                     0),
             new BlockFormatDirective(
                     "/*",
                     " * ",
                     null,
                     " */",
                     0)),
    CPP_HEADER("cpp:header",
             ".hpp",
             new Indenter("cpp-header", false, 10, 4, true),
             new BlockFormatDirective(
                     "/**",
                     " * ",
                     null,
                     " */",
                     0),
             new BlockFormatDirective(
                     "/*",
                     " * ",
                     null,
                     " */",
                     0)
    ),
    CPP_SOURCE("cpp:source",
             ".cpp",
             new Indenter("cpp-source", false, 10, 4, true),
             new BlockFormatDirective(
                     "/**",
                     " * ",
                     null,
                     " */",
                     0),
             new BlockFormatDirective(
                     "/*",
                     " * ",
                     null,
                     " */",
                     0)),
    MDL_META("meta",
              ".meta",
              new Indenter("meta", false, 10, 4, true),
              new BlockFormatDirective(
                      "# ",
                      "# ",
                      null,
                      "# ",
                      0),
              new BlockFormatDirective(
                      "# ",
                      "# ",
                      null,
                      "# ",
                      0)),
    AUTOMAKE("automake",
             ".am",
             new Indenter("automake", true, 10, 1, true),
             new BlockFormatDirective(
                     "# ",
                     "# ",
                     null,
                     "# ",
                     0),
             new BlockFormatDirective(
                     "# ",
                     "# ",
                     null,
                     "# ",
                     0)),
    AUTOCONFIG("autoconfig",
             ".ac",
             new Indenter("autoconfig", true, 10, 1, true),
             new BlockFormatDirective(
                     "# ",
                     "# ",
                     null,
                     "# ",
                     0),
             new BlockFormatDirective(
                     "# ",
                     "# ",
                     null,
                     "# ",
                     0)),

    SHELL("shell",
               ".sh",
               new Indenter("shell", true, 10, 1, true),
               new BlockFormatDirective(
                       "# ",
                       "# ",
                       null,
                       "# ",
                       0),
               new BlockFormatDirective(
                       "# ",
                       "# ",
                       null,
                       "# ",
                       0)),
    PKG_CONFIG_IN("pkgconfigin",
          ".pc.in",
          new Indenter("pkgconfigin", true, 10, 1, true),
          new BlockFormatDirective(
                  "# ",
                  "# ",
                  null,
                  "# ",
                  0),
          new BlockFormatDirective(
                  "# ",
                  "# ",
                  null,
                  "# ",
                  0)),
    SPEC_IN("specin",
            ".spec.in",
            new Indenter("specin", true, 10, 1, true),
            new BlockFormatDirective(
                    "# ",
                    "# ",
                    null,
                    "# ",
                    0),
                    new BlockFormatDirective(
                            "# ",
                            "# ",
                            null,
                            "# ",
                            0)),

    DOXYFILE_IN("doxyfile.in",
                  ".in",
                  new Indenter("doxyfile.in", true, 10, 1, true),
                  new BlockFormatDirective(
                          "# ",
                          "# ",
                          null,
                          "# ",
                          0),
                  new BlockFormatDirective(
                          "# ",
                          "# ",
                          null,
                          "# ",
                          0)),
    M4("m4",
        ".m4",
        new Indenter("m4", true, 10, 1, true),
        new BlockFormatDirective(
                "# ",
                "# ",
                null,
                "# ",
                0),
        new BlockFormatDirective(
                "# ",
                "# ",
                null,
                "# ",
                0)),
    DEBPKG_CONTROL("debpkg_control",
                   ".control",
                   new Indenter("debpkg_control", true, 10, 1, true),
                   new BlockFormatDirective(
                           "# ",
                           "# ",
                           null,
                           "# ",
                           0),
                   new BlockFormatDirective(
                           "# ",
                           "# ",
                           null,
                           "# ",
                           0)),
    DEBPKG_CHANGELOG("debpkg_changelog",
                     ".changelog",
                     new Indenter("debpkg_changelog", true, 10, 1, true),
                     new BlockFormatDirective(
                            "# ",
                            "# ",
                            null,
                            "# ",
                            0),
                     new BlockFormatDirective(
                            "# ",
                            "# ",
                            null,
                            "# ",
                            0)),
    DEBPKG_COMPAT("debpkg_compat",
                  ".compat",
                  new Indenter("debpkg_compat", true, 10, 1, true),
                  new BlockFormatDirective(
                          "# ",
                          "# ",
                          null,
                          "# ",
                          0),
                  new BlockFormatDirective(
                          "# ",
                          "# ",
                          null,
                          "# ",
                          0)),
    DEBPKG_COPYRIGHT("debpkg_copyright",
                     ".copyright",
                     new Indenter("debpkg_copyright", true, 10, 1, true),
                     new BlockFormatDirective(
                             "# ",
                             "# ",
                             null,
                             "# ",
                             0),
                     new BlockFormatDirective(
                             "# ",
                             "# ",
                             null,
                             "# ",
                             0)),
    DEBPKG_RULES("debpkg_rules",
                 ".rules",
                 new Indenter("debpkg_rules", true, 10, 1, true),
                 new BlockFormatDirective(
                         "# ",
                         "# ",
                         null,
                         "# ",
                         0),
                 new BlockFormatDirective(
                         "# ",
                         "# ",
                         null,
                         "# ",
                         0)),
    DEBPKG_FORMAT("debpkg_source_format",
                  ".source_format",
                  new Indenter("debpkg_source_format", true, 10, 1, true),
                  new BlockFormatDirective(
                          "# ",
                          "# ",
                          null,
                          "# ",
                          0),
                  new BlockFormatDirective(
                          "# ",
                          "# ",
                          null,
                          "# ",
                          0)),
    DEBPKG_DEV_INSTALL("debpkg_dev_install",
                       ".dev-install",
                       new Indenter("debpkg_dev_install", true, 10, 1, true),
                       new BlockFormatDirective(
                               "# ",
                               "# ",
                               null,
                               "# ",
                               0),
                       new BlockFormatDirective(
                               "# ",
                               "# ",
                               null,
                               "# ",
                               0)),
    DEBPKG_INSTALL("debpkg_install",
                   ".install",
                   new Indenter("debpkg_install", true, 10, 1, true),
                   new BlockFormatDirective(
                           "# ",
                           "# ",
                           null,
                           "# ",
                           0),
                   new BlockFormatDirective(
                           "# ",
                           "# ",
                           null,
                           "# ",
                           0)),
    JAVA_SOURCE("java:source",
                ".java",
                new Indenter("java-source", false, 10, 4, true),
                new BlockFormatDirective(
                        "/**",
                        " * ",
                        null,
                        " */",
                        0),
                new BlockFormatDirective(
                        "/*",
                        " * ",
                        null,
                        " */",
                        0)),
    MVN("mvn",
        ".xml",
        new Indenter("mvn", false, 10, 2, true),
        new BlockFormatDirective(
                "<!--",
                " ",
                null,
                "-->",
                0),
        new BlockFormatDirective(
                "<!--",
                " ",
                null,
                "-->",
                0)),
    ;
    private FileTypeMeta(String aInName,
                         String ainFileExt,
                         Indenter aInIndenter,
                         BlockFormatDirective aInHeaderFormatDirective,
                         BlockFormatDirective aInCommentFormatDirective)
    {
        name = aInName;
        fileExt = ainFileExt;
        indenter = aInIndenter;
        headerFormatDirective = aInHeaderFormatDirective;
        commentFormatDirective = aInCommentFormatDirective;
    }

    public String getName() { return name; }
    public String getFileExt() { return fileExt; }
    public Indenter getIndenter() { return indenter; }
    public BlockFormatDirective getHeaderFormatDirective() { return headerFormatDirective; }
    public BlockFormatDirective getCommentFormatDirective() { return commentFormatDirective; }

    public static FileTypeMeta get(String aIn)
    {
        for (FileTypeMeta lThis : FileTypeMeta.values())
        {
            if (lThis.name.equalsIgnoreCase(aIn))
            {
                return lThis;
            }
        }
        Severity.DEATH.report(
                "FileTypeMeta",
                "get file type meta for name",
                "no such file type meta",
                "no support for " + aIn);
        return null;
    }

    public String toString()
    {
        return "file-type-meta(" + name + ')';
    }
    private final String name;
    private final String fileExt;
    private final Indenter indenter;
    private final BlockFormatDirective headerFormatDirective;
    private final BlockFormatDirective commentFormatDirective;
}
