package org.opendaylight.opflex.genie.content.format.agent.build.automake.cpp;

import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.*;
import org.opendaylight.opflex.genie.engine.proc.Config;

/**
 * Created by midvorki on 10/7/14.
 */
public class FSpecIn
        extends GenericFormatterTask
{
    public FSpecIn(
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
        FileNameRule lFnr = new FileNameRule(
                aInFnr.getRelativePath(),
                null,
                aInFnr.getFilePrefix(),
                aInFnr.getFileSuffix(),
                aInFnr.getFileExtension(),
                lLibName);

        return lFnr;
    }

    public void firstLineCb()
    {
        out.println("");
    }


    public void generate()
    {
        String lModuleName = Config.getProjName();
        String lLibName = Config.getLibName();

        out.println(FORMAT.replaceAll("_MODULE_NAME_", lModuleName).replaceAll("_LIB_NAME_", lLibName));
    }
    
    private static final String FORMAT =
            "Name: _LIB_NAME_\n"
            + "Epoch:   1\n"
            + "Version: @VERSION@\n"
            + "Release: @SDK_BVERSION@%{?dist}\n"
            + "Summary: Generated _MODULE_NAME_ model for use with libopflex\n"
            + "\n"
            + "Group: Development/Libraries\n"
            + "License: Public Domain\n"
            + "URL: https://wiki.opendaylight.org/view/OpFlex:Main\n"
            + "\n"
            + "BuildRoot: %{_tmppath}/%{name}-%{version}-root\n"
            + "Source: %{name}-%{version}.tar.gz\n"
            + "Requires: libopflex >= %{version}\n"
            + "BuildRequires: libopflex-devel\n"
            + "BuildRequires: boost-devel\n"
            + "BuildRequires: doxygen\n"
            + "BuildRequires: devtoolset-7-toolchain\n"
            + "\n"
            + "%bcond_without check\n"
            + "\n"
            + "%description\n"
            + "Generated Opflex model definition\n"
            + "\n"
            + "%package devel\n"
            + "Summary: Development libraries for %{name}\n"
            + "Group: Development/Libraries\n"
            + "Requires: %{name} = %{epoch}:%{version}-%{release}\n"
            + "Requires: pkgconfig\n"
            + "Requires(post): /sbin/ldconfig\n"
            + "Requires(postun): /sbin/ldconfig\n"
            + "\n"
            + "%description devel\n"
            + "Development libraries for %{name}\n"
            + "\n"
            + "%prep\n"
            + "%setup -q\n"
            + "\n"
            + "%build\n"
            + ". /opt/rh/devtoolset-7/enable\n"
            + "%define __strip /opt/rh/devtoolset-7/root/usr/bin/strip\n"
            + "%configure\n"
            + "make %{?_smp_mflags}\n"
            + "\n"
            + "%install\n"
            + "%make_install\n"
            + "\n"
            + "%check\n"
            + "%if %{with check}\n"
            + "    . /opt/rh/devtoolset-7/enable\n"
            + "    %define __strip /opt/rh/devtoolset-7/root/usr/bin/strip\n"
            + "    make check\n"
            + "%endif\n"
            + "\n"
            + "%post -p /sbin/ldconfig\n"
            + "%postun -p /sbin/ldconfig\n"
            + "\n"
            + "%files\n"
            + "%defattr(-,root,root)\n"
            + "%{_libdir}/%{name}.so.*\n"
            + "\n"
            + "%files devel\n"
            + "%defattr(-,root,root)\n"
            + "%{_libdir}/%{name}.so\n"
            + "%{_libdir}/%{name}.la\n"
            + "%{_libdir}/%{name}.a\n"
            + "%{_libdir}/pkgconfig/%{name}.pc\n"
            + "%{_includedir}/_MODULE_NAME_/\n"
            + "%doc %{_docdir}/%{name}/\n"
            + "\n"
            + "%changelog\n"
            + "* Tue Dec 09 2014 Rob Adams <readams@readams.net> - 1:0.1.0\n"
            + "- New package file\n";


}
