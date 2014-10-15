package org.opendaylight.opflex.genie.content.model.mtype;

import org.opendaylight.opflex.modlan.report.Severity;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/28/14.
 */
public enum DefinedIn
{
    DEC,
    HEX,
    OCT,
    BIN,
    ASCII,
    STRING,
    SPECIAL,
    UNKNOWN
    ;
    public static DefinedIn get(String aIn)
    {
        if (Strings.isEmpty(aIn))
        {
            return UNKNOWN;
        }
        for (DefinedIn lThis : DefinedIn.values())
        {
            if (lThis.toString().equalsIgnoreCase(aIn))
            {
                return lThis;
            }
        }
        Severity.DEATH.report(
                "type:defined-in", "retrieval of defined-in directve", "", "no such defined-in directive: " + aIn);
        return null;
    }
}
