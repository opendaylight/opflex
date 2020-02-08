package org.opendaylight.opflex.genie.engine.format;

/**
 * Created by midvorki on 7/24/14.
 */
public class FormatterCtx
{
    public FormatterCtx(String aInDomain, String aInDestDir)
    {
        domain = aInDomain;
        rootPath = aInDestDir;
    }

    public String getRootPath()
    {
        return rootPath;
    }
    public String getDomain() { return domain; }

    public String toString()
    {
        return "formatter-ctx(" + getRootPath() + ')';
    }

    private final String domain;
    private final String rootPath;
}
