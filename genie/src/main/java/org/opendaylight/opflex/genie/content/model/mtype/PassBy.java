package org.opendaylight.opflex.genie.content.model.mtype;

import org.opendaylight.opflex.modlan.report.Severity;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/28/14.
 */
public enum PassBy
{
    VALUE,
    REFERENCE,
    POINTER,
    UNKNOWN
    ;
    public static PassBy get(String aIn)
    {
        if (Strings.isEmpty(aIn))
        {
            return UNKNOWN;
        }
        for (PassBy lThis : PassBy.values())
        {
            if (lThis.toString().equalsIgnoreCase(aIn))
            {
                return lThis;
            }
        }
        Severity.DEATH.report("type:pass-by", "retrieval of pass-by directve", "", "no such pass-by directive: " + aIn);
        return null;
    }
}
