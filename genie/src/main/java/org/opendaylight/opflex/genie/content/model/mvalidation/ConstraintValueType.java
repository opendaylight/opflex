package org.opendaylight.opflex.genie.content.model.mvalidation;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/10/14.
 */
public enum ConstraintValueType
{
    MIN("min"),
    MAX("max"),
    SIZE("size"),
    ;
    private ConstraintValueType(String aIn)
    {
        name = aIn;
    }

    public String getName()
    {
        return name;
    }

    public String toString()
    {
        return name;
    }

    public static ConstraintValueType get(String aIn)
    {
        for (ConstraintValueType lBCt : ConstraintValueType.values())
        {
            if (aIn.equalsIgnoreCase(lBCt.getName()))
            {
                return lBCt;
            }
        }
        Severity.DEATH.report(
                "ConstraintValueType",
                "get bounds constraint value type for name",
                "no such bounds constraint value type",
                "no support for " + aIn + "; bounds constraint value types supported: " + ConstraintValueType.values());

        return null;
    }
    private final String name;
}
