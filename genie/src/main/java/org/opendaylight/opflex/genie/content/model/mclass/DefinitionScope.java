package org.opendaylight.opflex.genie.content.model.mclass;

/**
 * Created by midvorki on 8/4/14.
 */
public enum DefinitionScope
{
    CLASS,
    MODULE,
    NONE
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
        return NONE;
    }
}
