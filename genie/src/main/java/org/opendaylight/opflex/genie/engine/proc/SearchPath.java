package org.opendaylight.opflex.genie.engine.proc;

/**
 * Created by midvorki on 10/22/14.
 */
public class SearchPath
{
    public SearchPath(String aInPath, String aInSuffix)
    {
        path = aInPath;
        suffix = aInSuffix;
    }

    public String getPath()
    {
        return path;
    }

    public String getSuffix()
    {
        return suffix;
    }

    public String[] toArray()
    {
        String[] lStrA = {path, suffix};
        return lStrA;
    }

    public String toString()
    {
        return "search-path(path=" + path + ", suffix=" + suffix + ")";
    }
    private final String path;
    private final String suffix;
}
