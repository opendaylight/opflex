package org.opendaylight.opflex.genie.content.model.mownership;

/**
 * Created by midvorki on 9/27/14.
 */
public enum DefinitionScope
{
    GLOBAL,
    OWNER,
    CLASS,
    MODULE,
    MEMBER,
    GROUP
    ;

    public static DefinitionScope get(String aIn)
    {
        for (DefinitionScope lScope : DefinitionScope.values())
        {
            if (aIn.equalsIgnoreCase(lScope.toString()))
            {
                return lScope;
            }
        }
        return GLOBAL;
    }
}
