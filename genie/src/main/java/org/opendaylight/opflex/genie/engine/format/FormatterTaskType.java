package org.opendaylight.opflex.genie.engine.format;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/24/14.
 */
public enum FormatterTaskType
{
    GENERIC("generic"),
    CATEGORY("category"),
    ITEM("item"),
    ;
    private FormatterTaskType(String aInName)
    {
        name = aInName;
    }
    public static FormatterTaskType get(String aIn)
    {
        for (FormatterTaskType lThis : FormatterTaskType.values())
        {
            if (lThis.name.equalsIgnoreCase(aIn))
            {
                return lThis;
            }
        }
        Severity.DEATH.report(
                "FormatterTaskType",
                "get formatter task type for name",
                "no such formatter task type",
                "no support for " + aIn);
        return null;
    }
    private final String name;
}
