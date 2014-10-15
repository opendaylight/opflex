package org.opendaylight.opflex.genie.content.model.mvalidation;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by dvorkinista on 7/10/14.
 */
public enum ValidatorScope
{
    /**
     * corresponding const is bound to a property
     */
    PROPERTY(new String[]{"member", "override", "prop", "property", "data"}),

    /**
     * corresponding const is bound to a type
     */
    TYPE(new String[]{"primitive", "type", "base"})
    ;

    private ValidatorScope(String[] aInModelNames)
    {
        modelNames = aInModelNames;
    }

    public static ValidatorScope get(String aInName)
    {
        for (ValidatorScope lThis : ValidatorScope.values())
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
                "get validation scope for name",
                "no such validation scope",
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
