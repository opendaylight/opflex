package org.opendaylight.opflex.genie.content.model.mconst;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by dvorkinista on 7/11/14.
 *
 * Identifies whether the corresponding constant is bound to property ot a type
 */
public enum ConstScope
{
    /**
     * corresponding const is bound to a property
     */
    PROPERTY(new String[]{"member", "override", "override-member", "prop", "property", "data"}),

    /**
     * corresponding const is bound to a type
     */
    TYPE(new String[]{"primitive", "type", "base"})
    ;

    private ConstScope(String[] aInModelNames)
    {
        modelNames = aInModelNames;
    }

    public static ConstScope get(String aInName)
    {
        for (ConstScope lThis : ConstScope.values())
        {
            for (String lThisName : lThis.modelNames)
            {
                if (lThisName.equalsIgnoreCase(aInName))
                {
                    return lThis;
                }
            }
        }
        Severity.DEATH.report(
                "ConstScope",
                "get const scope for name",
                "no such const scope",
                "no support for " + aInName);

        return null;
    }

    /**
     * stringifier
     */
    public String toString()
    {
        return super.toString().toLowerCase();
    }

    private final String[] modelNames;
}
