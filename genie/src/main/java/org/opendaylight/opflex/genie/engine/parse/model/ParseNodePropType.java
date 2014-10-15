package org.opendaylight.opflex.genie.engine.parse.model;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/22/14.
 */
public enum ParseNodePropType
{
    PROP("prop"),
    QUAL("qual"),
    OPTION("option"),
    ;

    private ParseNodePropType(String aInName)
    {
        name = aInName;
    }

    public static ParseNodePropType get(String aIn)
    {
        return get(aIn, true);
    }

    public static ParseNodePropType get(String aIn, boolean aInErrorIfNotFound)
    {
        for (ParseNodePropType lNPT : ParseNodePropType.values())
        {
            if (aIn.equalsIgnoreCase(lNPT.getName()))
            {
                return lNPT;
            }
        }
        if (aInErrorIfNotFound)
        {
            Severity.DEATH.report("ParseNodePropType", "get node property type for name", "no such property type",
                                  "no support for " + aIn + "; types supported: " + ParseNodePropType.values());
        }
        return null;
    }

    public String getName()
    {
        return name;
    }

    public String toString()
    {
        return name;
    }

    private final String name;
}
