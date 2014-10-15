package org.opendaylight.opflex.genie.content.model.mmeta;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/22/14.
 */
public enum NodeType
{
    NODE("node"),
    REFERENCE("ref")
    ;

    private NodeType(String aInModelName)
    {
        modelName = aInModelName;
    }

    public static NodeType get(String aInName)
    {
        for (NodeType lThis : NodeType.values())
        {
            if (lThis.modelName.equalsIgnoreCase(aInName))
            {
                return lThis;
            }
        }
        Severity.DEATH.report(
                "NodeType",
                "get node type for name",
                "no such node type",
                "no support for " + aInName);

        return null;
    }

    public String getName()
    {
        return modelName;
    }
    /**
     * stringifier
     */
    public String toString()
    {
        return modelName;
    }

    private final String modelName;
}
