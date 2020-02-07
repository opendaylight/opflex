package org.opendaylight.opflex.genie.content.model.mnaming;

/**
 * Created by midvorki on 7/10/14.
 *
 * Identifies scope of a name rule; whether it applies to a specific container context or all unspecified containments.
 */
public enum NameRuleScope
{
    /**
     * applies to any containing class that does not have specific rule
     */
    ANY("any")
    ;

    /**
     * Constructor
     * @param aIn scope name
     */
    NameRuleScope(String aIn)
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

    private final String name;

}
