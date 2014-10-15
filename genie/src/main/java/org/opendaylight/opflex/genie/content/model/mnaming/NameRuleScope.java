package org.opendaylight.opflex.genie.content.model.mnaming;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/10/14.
 *
 * Identifies scope of a name rule; whether it applies to a specific container context or all unspecified containments.
 */
public enum NameRuleScope
{
    /**
     * applies to a specific containing class
     */
    SPECIFIC_CLASS("class"),
    /**
     * applies to any containing class that does not have specific rule
     */
    ANY("any")
    ;

    /**
     * Constructor
     * @param aIn scope name
     */
    private NameRuleScope(String aIn)
    {
        name = aIn;
    }

    /**
     * name accessor
     * @return scope name
     */
    public String getName()
    {
        return name;
    }

    /**
     * stringifier
     * @return return name
     */
    public String toString()
    {
        return name;
    }

    /**
     * name to scope matcher. matches scope for a name passed in
     * @param aIn name
     * @return scope that matches the name
     */
    public static NameRuleScope get(String aIn)
    {
        for (NameRuleScope lNRS : NameRuleScope.values())
        {
            if (aIn.equalsIgnoreCase(lNRS.getName()))
            {
                return lNRS;
            }
        }
        Severity.DEATH.report(
                "NameRuleScope",
                "get name rule scope",
                "no such name rule scope",
                "no support for " + aIn + "; name rule scopes supported: " + NameRuleScope.values());

        return null;
    }
    private final String name;

}
