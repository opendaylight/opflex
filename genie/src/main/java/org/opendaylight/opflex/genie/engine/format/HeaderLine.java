package org.opendaylight.opflex.genie.engine.format;

import org.opendaylight.opflex.modlan.report.Severity;

import java.util.Collection;
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

    public Collection<HeaderOption> getOptions() { return options; }

    public boolean hasOption(HeaderOption aIn)
    {
        return options.contains(aIn);
    }

    private String line;
    private Set<HeaderOption> options = new TreeSet<>();
}
