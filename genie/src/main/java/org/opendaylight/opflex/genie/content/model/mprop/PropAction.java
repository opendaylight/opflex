package org.opendaylight.opflex.genie.content.model.mprop;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by dvorkinista on 7/7/14.
 */
public enum PropAction
{
    DEFINE(new String[] {"define", "member"}),
    OVERRIDE(new String[] {"override", "clobber", "override-member", "clobber-member"}),
    HIDE(new String[] {"hide", "remove", "remove-member", "hide-member"}),
    ;
    private PropAction(String[] aIn)
    {
        names = aIn;
    }

    public boolean isOverride()
    {
        return DEFINE != this;
    }

    public boolean isHide()
    {
        return HIDE == this;
    }

    public boolean isDefine()
    {
        return DEFINE == this;
    }

    public String getName()
    {
        return names[0];
    }

    public String toString()
    {
        return getName();
    }

    public static PropAction get(String aIn)
    {
        for (PropAction lPA : PropAction.values())
        {
            for (String lThis : lPA.names)
            {
                if (aIn.equalsIgnoreCase(lThis))
                {
                    return lPA;
                }
            }
        }
        Severity.DEATH.report(
                "PropertyAction",
                "get property action for name",
                "no such property action",
                "no support for " + aIn);

        return null;
    }
    private final String[] names;
}
