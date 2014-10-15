package org.opendaylight.opflex.genie.engine.format;

import org.opendaylight.opflex.genie.engine.file.WriteStats;

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

    public WriteStats getStats() { return stats; }

    public String toString()
    {
        return "formatter-ctx(" + getRootPath() + ')';
    }

    private final String domain;
    private final String rootPath;
    private static final WriteStats stats = new WriteStats();
}
