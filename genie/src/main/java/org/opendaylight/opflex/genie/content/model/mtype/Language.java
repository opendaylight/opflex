package org.opendaylight.opflex.genie.content.model.mtype;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by dvorkinista on 7/7/14.
 */
public enum Language
{
    C("c"),
    CPP("cpp"),
    ;
    private Language(String aIn)
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

    public static Language get(String aIn)
    {
        for (Language lLang : Language.values())
        {
            if (aIn.equalsIgnoreCase(lLang.getName()))
            {
                return lLang;
            }
        }
        Severity.DEATH.report(
                "Language", "get language enum for name", "language not supported", "no support for " + aIn);

        return null;
    }
    private final String name;
}
