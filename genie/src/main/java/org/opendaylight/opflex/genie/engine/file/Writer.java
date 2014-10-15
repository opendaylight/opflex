package org.opendaylight.opflex.genie.engine.file;

import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/23/14.
 */
public class Writer extends java.io.Writer
{
    /**
     * Constructor
     *
     * @param aInFile The file to be generated.
     */
    public Writer(File aInFile, WriteStats aInStats)
    {
        stats = aInStats;
        try
        {
            stats.incrTotal();

            file = aInFile;
            if (aInFile.exists())
            {
                stats.incrDuplicate();
                reader = new FileReader(aInFile);
            }
            else
            {
                stats.incrNew();
                writer = new FileWriter(aInFile);
            }
        }
        catch (Throwable lE)
        {
            Severity.DEATH.report(aInFile.toString(),"writer allocation", "unexpected error encountered", lE);
        }
    }

    public void write(char aInCbuf[], int aInOff, int aInLen)
            throws
            IOException
    {
        if (reader != null)
        {
            char[] buffer = null;
            try
            {
                buffer = new char[aInLen];
                if (reader.read(buffer, 0, aInLen) == aInLen)
                {
                    // got expected amount of input, check if contents equal
                    int lCbufIndex = aInOff + aInLen;
                    int lBufferIndex = aInLen;
                    while (lBufferIndex > 0)
                    {
                        if (aInCbuf[--lCbufIndex] != buffer[--lBufferIndex])
                        {
                            differ();
                            break;
                        }
                    }
                }
                else
                {
                    differ();
                }
            }
            catch (OutOfMemoryError e)
            {
                differ();
            }
            if (writer == null)
            {
                // not writing, chunk must be the same as one to write.
                if (oldBuffers == null)
                {
                    oldBuffers = new ArrayList<char[]>();
                }
                oldBuffers.add(buffer);
                buffer = null;
            }
        }
        if (writer != null)
        {
            writer.write(aInCbuf, aInOff, aInLen);
        }
    }

    public void flush()
            throws
            IOException
    {
        if (writer != null)
        {
            writer.flush();
        }
    }

    public void close()
            throws
            IOException
    {
        if (reader != null && reader.ready())
        {
            // done writing output, still stuff in input - file was truncated!
            differ();
        }
        if (reader != null)
        {
            reader.close();
        }
        if (writer != null)
        {
            writer.close();
        }
    }

    /**
     * Called when a mismatch was found between the input and output stream,
     * setup the writer stream, and insert any previously matched input.
     *
     * @throws IOException If there was a problem opening the writer.
     */
    private void differ()
            throws
            IOException
    {
        stats.decrDuplicate();
        stats.incrChanged();
        reader.close();
        reader = null;

        writer = new FileWriter(file);
        if (oldBuffers != null)
        {
            for (char[] buffer : oldBuffers)
            {
                writer.write(buffer, 0, buffer.length);
            }
            oldBuffers = null;
        }
    }

    /**
     * The file which is being processed.
     */
    private File file;
    /**
     * The input for an existing file which is being compared against.
     */
    private FileReader reader;
    /**
     * The output for a file which is new or different.
     */
    private FileWriter writer;
    /**
     * The list of previously read buffers that were encountered.
     */
    private ArrayList<char[]> oldBuffers;

    private final WriteStats stats;
}
