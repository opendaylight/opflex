package org.opendaylight.opflex.genie.engine.format;

import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/24/14.
 *
 * Specifies name formation rule for generated file
 * <*projectpath*>/ralativepath/modulepath/[fileprefix][*FILENAME*][filesuffix][.filextension]
 */
public class FileNameRule
{
    /**
     * Constructor
     * <*projectpath*>/ralativepath/modulepath/[fileprefix][*FILENAME*][filesuffix][.filextension]
     */
    public FileNameRule(
            final String aInRelativePath,
            final String aInModulePath,
            final String aInFilePrefix,
            final String aInFileSuffix,
            final String aInFileExtension,
            final String aInName)
    {
        relativePath = aInRelativePath;
        modulePath = aInModulePath;
        filePrefix = aInFilePrefix;
        fileSuffix = aInFileSuffix;
        fileExtension = aInFileExtension;
        name = aInName;
    }

    public final String getName() { return name; }
    public final String getRelativePath()
    {
        return relativePath;
    }
    public final String getModulePath()
    {
        return modulePath;
    }
    public final String getFilePrefix()
    {
        return filePrefix;
    }
    public final String getFileSuffix()
    {
        return fileSuffix;
    }
    public final String getFileExtension()
    {
        return fileExtension;
    }
    public final boolean isTemplate() { return Strings.isEmpty(modulePath) || Strings.isEmpty(name); }

    public FileNameRule makeSpecific(String aInModulePath, String aInName)
    {
        return !(Strings.isEmpty(aInModulePath) && Strings.isEmpty(aInName)) ?
                    new FileNameRule(
                            relativePath,
                            Strings.isEmpty(aInModulePath) ? modulePath : aInModulePath,
                            filePrefix,
                            fileSuffix,
                            fileExtension,
                            Strings.isEmpty(aInName) ? name : aInName) :
                    this;
    }

    private final String relativePath;
    private final String modulePath;
    private final String filePrefix;
    private final String fileSuffix;
    private final String fileExtension;
    private final String name;
}
