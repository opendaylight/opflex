package org.opendaylight.opflex.genie.engine.format;

import org.opendaylight.opflex.modlan.report.Severity;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.Collection;
import java.util.LinkedList;

/**
 * Created by midvorki on 11/5/14.
 */
public class Header
{
    public Header()
    {

    }

    public Header(String aInPath)
    {
        load(aInPath);
    }


    public void load(String aInPath)
    {
        File lFile = new File(aInPath);
        if (lFile.exists())
        {
            try
            {
                //Create object of FileReader
                FileReader lInputFile = new FileReader(lFile);

                //Instantiate the BufferedReader Class
                BufferedReader lBufferReader = new BufferedReader(lInputFile);

                //Variable to hold the one line data
                String lLine;

                // Read file line by line and print on the console
                while ((lLine = lBufferReader.readLine()) != null)
                {
                    add(lLine);
                }
                lBufferReader.close();
            }
            catch(Throwable lT)
            {
                Severity.DEATH.report("default header load", "","", lT);
            }
        }
        else
        {
            Severity.DEATH.report("default header load", "","", "file " + aInPath + " does not exist");
        }
    }

    public void add(String aIn)
    {
        lines.add(new HeaderLine(aIn));
    }

    public int getSize()
    {
        return lines.size();
    }

    public Collection<HeaderLine> get() { return lines; }

    private final Collection<HeaderLine> lines = new LinkedList<>();
}
