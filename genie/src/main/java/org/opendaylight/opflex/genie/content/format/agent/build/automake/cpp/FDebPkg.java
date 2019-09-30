package org.opendaylight.opflex.genie.content.format.agent.build.automake.cpp;

import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.*;
import org.opendaylight.opflex.genie.engine.proc.Config;
import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Formatter to create Debian packaging files
 */
public class FDebPkg
        extends GenericFormatterTask
{
    public FDebPkg(
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
        String lLibName = Config.getLibName();
        String suffix = aInFnr.getFileSuffix();
        String ext = aInFnr.getFileExtension();
        if (ext.equals(".control")) {
            ext = "";
            lLibName = "control";
        } else if (ext.equals(".compat")) {
            ext = "";
            lLibName = "compat";
        } else if (ext.equals(".changelog")) {
            ext = "";
            lLibName = "changelog.in";
        } else if (ext.equals(".copyright")) {
            ext = "";
            lLibName = "copyright";
        } else if (ext.equals(".rules")) {
            ext = "";
            lLibName = "rules.in";
        } else if (ext.equals(".source_format")) {
            ext = "";
            lLibName = "format";
        } else if (ext.equals(".dev-install")) {
            suffix = "-dev";
            ext = ".install";
        }
        FileNameRule lFnr = new FileNameRule(
                aInFnr.getRelativePath(),
                null,
                aInFnr.getFilePrefix(),
                suffix,
                ext,
                lLibName);
        return lFnr;
    }

    public void firstLineCb()
    {
        String type = getMeta().getFile().getName();
        if (type.equals("debpkg_compat")) {
            out.println(TEMPLATE_COMPAT);
        } else if (type.equals("debpkg_source_format")) {
            out.println(TEMPLATE_FORMAT);
        } else if (type.equals("debpkg_rules")) {
            out.println(TEMPLATE_RULES_HEADER);
        }
    }


    public void generate()
    {
        String lModuleName = Config.getProjName();
        String lLibName = Config.getLibName();

        out.println(getTemplate()
                    .replaceAll("_MODULE_NAME_", lModuleName)
                    .replaceAll("_LIB_NAME_", lLibName));
    }

    private String getTemplate() {
        String type = getMeta().getFile().getName();
        if (type.equals("debpkg_control")) {
            return TEMPLATE_CONTROL;
        } else if (type.equals("debpkg_changelog")) {
            return TEMPLATE_CHANGELOG;
        } else if (type.equals("debpkg_compat")) {
            return "";
        } else if (type.equals("debpkg_copyright")) {
            return TEMPLATE_COPYRIGHT;
        } else if (type.equals("debpkg_rules")) {
            return TEMPLATE_RULES_BODY;
        } else if (type.equals("debpkg_source_format")) {
            return "";
        } else if (type.equals("debpkg_dev_install")) {
            return TEMPLATE_DEV_INSTALL;
        } else if (type.equals("debpkg_install")) {
            return TEMPLATE_INSTALL;
        } else {
            Severity.DEATH.report(
                "FDebPkg",
                "get file template for file type",
                "unknown file type",
                "no support for " + type);
            return null;
        }
    }

    private static final String TEMPLATE_CONTROL =
        "Source: _LIB_NAME_\n"
        + "Priority: optional\n"
        + "Maintainer: OpFlex Developers <opflex-dev@lists.opendaylight.org>\n"
        + "Build-Depends: debhelper (>= 8.0.0), autotools-dev,\n"
        + " libopflex-dev, libboost-dev, doxygen, pkgconf\n"
        + "Standards-Version: 3.9.8\n"
        + "Section: libs\n"
        + "Homepage: https://wiki.opendaylight.org/view/OpFlex:Main\n"
        + "\n"
        + "Package: _LIB_NAME_-dev\n"
        + "Section: libdevel\n"
        + "Architecture: any\n"
        + "Multi-Arch: same\n"
        + "Pre-Depends: ${misc:Pre-Depends}\n"
        + "Depends: _LIB_NAME_ (= ${binary:Version})\n"
        + "Description: Development libraries for _LIB_NAME_\n"
        + " Development libraries for _LIB_NAME_\n"
        + "\n"
        + "Package: _LIB_NAME_\n"
        + "Section: libs\n"
        + "Architecture: any\n"
        + "Depends: libopflex (>= ${binary:Version}), ${shlibs:Depends}, ${misc:Depends}\n"
        + "Description: Generated _MODULE_NAME_ model for use with libopflex\n"
        + " Generated Opflex model definition\n"
        + "\n"
        + "Package: _LIB_NAME_-dbg\n"
        + "Section: debug\n"
        + "Architecture: any\n"
        + "Priority: extra\n"
        + "Depends: _LIB_NAME_ (= ${binary:Version}), ${misc:Depends}\n"
        + "Description: Debug symbols for _LIB_NAME_ package\n"
        + " Generated Opflex model definition\n"
        + " .\n"
        + " This package contains the debugging symbols for _LIB_NAME_.";
    private static final String TEMPLATE_CHANGELOG =
        "_LIB_NAME_ (@PACKAGE_VERSION@-@SDK_BVERSION@) unstable; urgency=low\n"
        + "\n"
        + "  * Initial release\n"
        + "\n"
        + " -- Amit Bose <bose@noironetworks.com>  Tue, 24 Mar 2015 12:52:06 -0700\n";
    private static final String TEMPLATE_COMPAT = "9\n";
    private static final String TEMPLATE_COPYRIGHT =
        "Format: http://www.debian.org/doc/packaging-manuals/copyright-format/1.0/\n"
        + "Upstream-Name: _LIB_NAME_\n"
        + "Source: https://wiki.opendaylight.org/view/OpFlex:Main\n"
        + "\n"
        + "Files: *\n"
        + "Copyright: No copyright holder\n"
        + "License: General Public License\n";
    private static final String TEMPLATE_RULES_HEADER =
        "#!/usr/bin/make -f\n"
        + "# -*- makefile -*-\n";
    private static final String TEMPLATE_RULES_BODY =
        "\n"
        + "# Uncomment this to turn on verbose mode.\n"
        + "#export DH_VERBOSE=1\n"
        + "\n"
        + "%:\n"
        +"\tdh $@ --parallel --with autotools-dev\n"
        + "\n"
        + "#override_dh_auto_test:\n"
        + "\n"
        + "override_dh_auto_configure:\n"
        + "\tdh_auto_configure -- --disable-assert --with-buildversion=@SDK_BVERSION@\n"
        + "\n"
        + "override_dh_auto_install:\n"
        + "\t$(MAKE) DESTDIR=$(CURDIR)/debian/tmp install\n"
        + "\n"
        + "override_dh_shlibdeps:\n"
        + "\tdh_shlibdeps -- --ignore-missing-info\n"
        + "\n"
        + "override_dh_strip:\n"
        + "\tdh_strip --dbg-package=_LIB_NAME_-dbg";
    private static final String TEMPLATE_FORMAT = "3.0 (quilt)\n";
    private static final String TEMPLATE_DEV_INSTALL =
        "usr/include/*\n"
        + "usr/lib/*/lib*.a\n"
        + "usr/lib/*/lib*.la\n"
        + "usr/lib/*/pkgconfig/*\n"
        + "usr/share/doc/_LIB_NAME_/* usr/share/doc/_MODULE_NAME_/\n";
    private static final String TEMPLATE_INSTALL = "usr/lib/*/lib*.so*\n";
}
