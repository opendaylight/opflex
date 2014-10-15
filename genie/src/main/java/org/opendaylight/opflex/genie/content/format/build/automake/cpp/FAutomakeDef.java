package org.opendaylight.opflex.genie.content.format.build.automake.cpp;

import java.util.Collection;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.module.Module;
import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.*;
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
        generate(0, MClass.getModulesWithConcreteClasses(), lLibName);
    }

    public void generate(int ainIndent, Collection<Pair<Module, Collection<MClass>>> aInModules, String aInLibName)
    {
        out.println(ainIndent, "ACLOCAL_AMFLAGS = -I m4");
        out.println();

        /*
        policy_includedir = $(includedir)/gbpmodel/policy
        policy_include_HEADERS = \
    	    include/gbpmodel/policy/Universe.hpp \
	        include/gbpmodel/policy/Space.hpp
         */
        StringBuilder moddirs = new StringBuilder(" ");
        for (Pair<Module,Collection<MClass>> lModuleNode : aInModules)
        {
            Module lModule = lModuleNode.getFirst();

            String lModName = lModule.getLID().getName().toLowerCase();
            moddirs.append("$(" + lModName + "_include_HEADERS) ");
            out.println(ainIndent, lModName + "_includedir = $(includedir)/" + Config.getProjName() + "/" + lModName);

            Collection<MClass> lClasses = lModuleNode.getSecond();

            out.println(ainIndent, lModName + "_include_HEADERS = \\");
            for (MClass lClass : lClasses)
            {
                out.println(" \\");
                out.print(ainIndent + 1, "include/" + Config.getProjName() + "/" + lModName + "/" + Strings.upFirstLetter(lClass.getLID().getName()) + ".hpp");
            }

            out.println();
            out.println();
        }

        out.println(ainIndent, "metadata_includedir = $(includedir)/" + Config.getProjName() + "/metadata");
        out.println(ainIndent, "metadata_include_HEADERS = \\");
        out.print(ainIndent + 1, "include/" + Config.getProjName() + "/metadata/metadata.hpp");
        out.println();

        out.println(ainIndent, "AM_CPPFLAGS = -I$(top_srcdir)/include");
        out.println();
        out.println(ainIndent, "lib_LTLIBRARIES = " + aInLibName + ".la");
        out.println(ainIndent, aInLibName + "_la_SOURCES = src/metadata.cpp");
        out.println();
        out.println(ainIndent, "pkgconfigdir = $(libdir)/pkgconfig");
        out.println(ainIndent, "pkgconfig_DATA = " + aInLibName + ".pc");
        out.println();
        out.println(ainIndent ,"if HAVE_DOXYGEN");
            out.println(ainIndent + 1, "noinst_DATA = \\");
                        out.println(ainIndent + 2, "doc/html");
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
        out.println(ainIndent + 1,"rm -f *.rpm");
    }
}