package org.opendaylight.opflex.genie.engine.format;

import java.util.Set;
import java.util.TreeSet;

/**
 * Created by midvorki on 11/5/14.
 */
public class HeaderLine
{
    public HeaderLine(String aIn)
    {
        line = aIn;

        for (HeaderOption lOpt : HeaderOption.values())
        {
            if (line.contains(lOpt.getName()))
            {
                options.add(lOpt);
            }
        }
    }

    public String getLine()
    {
        return line;
    }

    public boolean hasOption(HeaderOption aIn)
    {
        return options.contains(aIn);
    }

    private final String line;
    private final Set<HeaderOption> options = new TreeSet<>();
}
