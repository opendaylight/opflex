package org.opendaylight.opflex.genie.engine.file;

import java.io.File;
import java.net.URI;
import java.util.Collection;
import java.util.TreeMap;
import java.util.Arrays;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 3/25/14.
 */
public class Lister
{
    public Lister(
        String aInPath,
        String aInSuffix
        )
    {
        explore(new File(aInPath), aInSuffix);
    }

    public Collection<File> getFiles()
    {
        return files.values();
    }

    private void explore(File aInFile, String aInSuffix)
    {
        if (aInFile.exists())
        {
            if (aInFile.isDirectory())
            {
                File[] lFiles = aInFile.listFiles();
                Arrays.sort(lFiles);
                for (File lThisF : lFiles)
                {
                    explore(lThisF, aInSuffix);
                }
            }
            else if (aInFile.isFile())
            {
                if (null == aInSuffix || 0 == aInSuffix.length() ||
                    aInFile.getPath().endsWith(aInSuffix))
                {
                    files.put(aInFile.toURI(), aInFile);
                }
            }
            else
            {
                Severity.WARN.report(aInFile.toURI().toString(), "model file search", "unknown file", "not a file.");
            }
        }
        else
        {
            Severity.DEATH.report(aInFile.toURI().toString(), "model file search", "no such file", "file does not seem to exist.");
        }
    }

    private final TreeMap<URI, File> files = new TreeMap<>();
}
