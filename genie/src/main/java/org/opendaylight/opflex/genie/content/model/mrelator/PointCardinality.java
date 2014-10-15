package org.opendaylight.opflex.genie.content.model.mrelator;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 8/6/14.
 */
public enum PointCardinality
{
    SINGLE,
    MANY,
    ;
    public static PointCardinality get(String aIn)
    {
        for (PointCardinality lThis : PointCardinality.values())
        {
            if (lThis.toString().equalsIgnoreCase(aIn))
            {
                return lThis;
            }
        }
        Severity.DEATH.report("relationship-cardinality", "get", "", "no cardinality: " + aIn);
        return SINGLE;
    }
}
