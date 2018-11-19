package org.opendaylight.opflex.genie.content.format.agent.build.automake.cpp;

import java.util.Collection;

import org.opendaylight.opflex.genie.content.format.agent.consts.cpp.FEnumDef;
import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.module.Module;
import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.BlockFormatDirective;
import org.opendaylight.opflex.genie.engine.format.FileNameRule;
import org.opendaylight.opflex.genie.engine.format.FormatterCtx;
import org.opendaylight.opflex.genie.engine.format.GenericFormatterTask;
import org.opendaylight.opflex.genie.engine.format.Indenter;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.proc.Config;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 10/6/14.
 */
public class FAutomakeDef
        extends GenericFormatterTask
{
    public FAutomakeDef(
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
                "Makefile");

        return lFnr;
    }

    public void generate()
    {
        String lLibName = Config.getLibName();
        generate(0, MClass.getModulesWithDefinables(), lLibName);
    }

    public void generate(int ainIndent, Collection<Pair<Module, Collection<Item>>> aInModules, String aInLibName)
    {
        out.println(ainIndent, "ACLOCAL_AMFLAGS = -I m4");
        out.println();

        out.println(ainIndent, "EXTRA_DIST =");
        out.println(ainIndent, "EXTRA_DIST += debian");
        out.println();

        /*
        policy_includedir = $(includedir)/gbpmodel/policy
        policy_include_HEADERS = \
    	    include/gbpmodel/policy/Universe.hpp \
	        include/gbpmodel/policy/Space.hpp
         */
        StringBuilder moddirs = new StringBuilder(" ");
        for (Pair<Module,Collection<Item>> lModuleNode : aInModules)
        {
            Module lModule = lModuleNode.getFirst();

            String lModName = lModule.getLID().getName().toLowerCase();
            moddirs.append("$(" + lModName + "_include_HEADERS) ");
            out.println(ainIndent, lModName + "_includedir = $(includedir)/" + Config.getProjName() + "/" + lModName);

            Collection<Item> lItems = lModuleNode.getSecond();

            out.print(ainIndent, lModName + "_include_HEADERS =");
            for (Item lItem : lItems)
            {
                out.println(" \\");
                if (lItem instanceof MClass)
                {
                    out.print(ainIndent + 1, "include/" + Config.getProjName() + "/" + lModName + "/" + Strings
                                      .upFirstLetter(lItem.getLID().getName()) + ".hpp");
                }
                else
                {
                    out.print(ainIndent + 1, "include/" + Config.getProjName() + "/" + lModName + "/" + FEnumDef.getClassName(lItem,false) + ".hpp");
                }
            }

            out.println();
            out.println();
        }

        out.println(ainIndent, "metadata_includedir = $(includedir)/" + Config.getProjName() + "/metadata");
        out.println(ainIndent, "metadata_include_HEADERS = \\");
        out.print(ainIndent + 1, "include/" + Config.getProjName() + "/metadata/metadata.hpp");
        out.println();

        out.println(ainIndent, "lib_LTLIBRARIES = " + aInLibName + ".la");
        out.println(ainIndent, aInLibName + "_la_CXXFLAGS = -fno-var-tracking-assignments -I$(top_srcdir)/include $(libopflex_CFLAGS)");
        out.println(ainIndent, aInLibName + "_la_SOURCES = src/metadata.cpp");
        out.println(ainIndent, aInLibName + "_la_LIBADD = $(libopflex_LIBS)");
        out.println(ainIndent, aInLibName + "_la_LDFLAGS = -version-info ${VERSION_INFO}");
        out.println();
        out.println(ainIndent, "pkgconfigdir = $(libdir)/pkgconfig");
        out.println(ainIndent, "pkgconfig_DATA = " + aInLibName + ".pc");
        out.println();
        out.println(ainIndent ,"if HAVE_DOXYGEN");
            out.println(ainIndent, "    noinst_DATA = \\");
                        out.println(ainIndent + 1, "doc/html");
        out.println(ainIndent ,"endif");
        out.println();
        out.println(ainIndent,"doc/html: $(metadata_include_HEADERS) " + 
                    moddirs.toString() + "doc/Doxyfile");
            out.println(ainIndent + 1,"cd doc && ${DOXYGEN} Doxyfile");
        out.println(ainIndent,"doc: doc/html");
        out.println(ainIndent,"install-data-local: doc");
            out.println(ainIndent + 1,"@$(NORMAL_INSTALL)");
            out.println(ainIndent + 1, "test -z \"${DESTDIR}/${docdir}/html\" || $(mkdir_p) \"${DESTDIR}/${docdir}/html\"");
            out.println(ainIndent + 1, "for i in `ls $(srcdir)/doc/html`; do \\");
                out.println(ainIndent + 2, "$(INSTALL) -m 0644 $(srcdir)/doc/html/$$i \"${DESTDIR}/${docdir}/html\"; \\");
            out.println(ainIndent + 1, "done");
        out.println(ainIndent,"uninstall-local:");
            out.println(ainIndent + 1,"@$(NORMAL_UNINSTALL)");
            out.println(ainIndent + 1,"rm -rf \"${DESTDIR}/${docdir}/html\"");
            out.println(ainIndent + 1,"rm -rf \"${DESTDIR}/${includedir}/" + Config.getProjName() + "\"");
        out.println(ainIndent,"clean-doc:");
        out.println(ainIndent + 1,"rm -rf doc/html doc/latex");
        out.println(ainIndent,"clean-local: clean-doc");
        out.println(ainIndent + 1,"rm -f *.rpm *.deb");
        out.println();

        out.println(ainIndent,"CWD=`pwd`");
        out.println(ainIndent,"RPMFLAGS=--define \"_topdir ${CWD}/rpm\"");
        out.println(ainIndent,"ARCH=x86_64");
        out.println(ainIndent,"SOURCE_FILE=${PACKAGE}-${VERSION}.tar.gz");
        out.println(ainIndent,"RPMDIRS=rpm/BUILD rpm/SOURCES rpm/RPMS rpm/SRPMS");
        out.println(ainIndent,"rpm: dist rpm/${PACKAGE}.spec");
        out.println(ainIndent + 1,"mkdir -p ${RPMDIRS}");
        out.println(ainIndent + 1,"cp ${SOURCE_FILE} rpm/SOURCES/");
        out.println(ainIndent + 1,"rpmbuild ${RPMFLAGS} -ba rpm/${PACKAGE}.spec");
        out.println(ainIndent + 1,"cp rpm/RPMS/${ARCH}/*.rpm .");
        out.println(ainIndent + 1,"cp rpm/SRPMS/*.rpm .");
        out.println(ainIndent + 1,"rm -rf ${RPMDIRS}");
        out.println(ainIndent,"srpm: dist rpm/${PACKAGE}.spec");
        out.println(ainIndent + 1,"mkdir -p ${RPMDIRS}");
        out.println(ainIndent + 1,"cp ${SOURCE_FILE} rpm/SOURCES/");
        out.println(ainIndent + 1,"rpmbuild ${RPMFLAGS} -bs rpm/${PACKAGE}.spec");
        out.println(ainIndent + 1,"cp rpm/SRPMS/*.rpm .");
        out.println(ainIndent + 1,"rm -rf ${RPMDIRS}");
        out.println();

        out.println(ainIndent, "# Set env var DEB_BUILD_OPTIONS=\"parallel=<#cores>\" to speed up package builds");
        out.println(ainIndent, "DEB_PKG_DIR=deb-pkg-build");
        out.println(ainIndent, "deb: dist");
        out.println(ainIndent + 1, "- rm -rf  $(DEB_PKG_DIR)");
        out.println(ainIndent + 1, "mkdir -p $(DEB_PKG_DIR)");
        out.println(ainIndent + 1, "cp $(SOURCE_FILE) $(DEB_PKG_DIR)/");
        out.println(ainIndent + 1, "tar -C $(DEB_PKG_DIR)/ -xf $(DEB_PKG_DIR)/$(SOURCE_FILE)");
        out.println(ainIndent + 1, "mv $(DEB_PKG_DIR)/$(SOURCE_FILE) $(DEB_PKG_DIR)/$(PACKAGE)_$(VERSION).orig.tar.gz");
        out.println(ainIndent + 1, "cd $(DEB_PKG_DIR)/$(PACKAGE)-$(VERSION)/; " +
                                   "dpkg-buildpackage -us -uc -rfakeroot -b");
        out.println(ainIndent + 1, "cp $(DEB_PKG_DIR)/*.deb .");
        out.println(ainIndent + 1, "rm -rf $(DEB_PKG_DIR)");

        out.println(ainIndent, "dsc: dist");
        out.println(ainIndent + 1, "- rm -rf  $(DEB_PKG_DIR)");
        out.println(ainIndent + 1, "mkdir -p $(DEB_PKG_DIR)");
        out.println(ainIndent + 1, "cp $(SOURCE_FILE) $(DEB_PKG_DIR)/");
        out.println(ainIndent + 1, "tar -C $(DEB_PKG_DIR)/ -xf $(DEB_PKG_DIR)/$(SOURCE_FILE)");
        out.println(ainIndent + 1, "mv $(DEB_PKG_DIR)/$(SOURCE_FILE) $(DEB_PKG_DIR)/$(PACKAGE)_$(VERSION).orig.tar.gz");
        out.println(ainIndent + 1, "cd $(DEB_PKG_DIR)/$(PACKAGE)-$(VERSION)/; " +
                                   "dpkg-buildpackage -d -us -uc -rfakeroot -S");
    }
}
