package org.opendaylight.opflex.genie.engine.parse.load;

/**
 * Created by midvorki on 7/25/14.
 */
public enum LoadStage
{
    PRE(false),
    LOAD(true),
    POST(true)
    ;
    private LoadStage(boolean aInIsParallel)
    {
        isParallel = aInIsParallel;
    }

    public static LoadStage get(String aInName)
    {
        for (LoadStage lThis : LoadStage.values())
        {
            if (lThis.toString().equalsIgnoreCase(aInName))
            {
                return lThis;
            }
        }
        return LOAD;
    }

    public boolean isParallel()
    {
        return isParallel;
    }

    private final boolean isParallel;
}
