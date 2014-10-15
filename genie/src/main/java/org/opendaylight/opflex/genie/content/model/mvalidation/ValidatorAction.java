package org.opendaylight.opflex.genie.content.model.mvalidation;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/10/14.
 */
public enum ValidatorAction
{
    ADD(new String[]{"add", "validate", "range", "content"}),
    CLOBBER(new String[]{"clobber", "clobber-validate", "clobber-range", "clobber-content"}),
    REMOVE(new String[]{"remove", "remove-validate", "remove-range", "remove-content"}),
    ;

    private ValidatorAction(String[] aIn)
    {
        names = aIn;
    }

    public String getName()
    {
        return names[0];
    }

    public String toString()
    {
        return getName();
    }

    public static ValidatorAction get(String aIn)
    {
        for (ValidatorAction lVA : ValidatorAction.values())
        {
            for (String lS : lVA.names)
            {
                if (lS.equalsIgnoreCase(aIn))
                {
                    return lVA;
                }
            }
        }
        Severity.DEATH.report(
                "ValidatorAction",
                "get validator action for name",
                "no such validator action",
                "no support for " + aIn + "; actions supported: " + ValidatorAction.values());

        return null;
    }

//    public MConstraintValue
    private final String[] names;
}
