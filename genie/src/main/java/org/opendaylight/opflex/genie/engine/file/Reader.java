package org.opendaylight.opflex.genie.engine.file;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 3/25/14.
 */
public class Reader implements org.opendaylight.opflex.modlan.parse.Ctx
{
    public static final int BUFF_SIZE = 1024;

    public Reader(File aInFile)
    {
        file = aInFile;
        is = fileInputStreamFactory(aInFile);
        isr = new InputStreamReader(is);
    }

    public boolean hasMore()
    {
        return holdThis || check();
    }

    public char getThis()
    {
        return buffer[bufferIdx];
    }

    public void holdThisForNext()
    {
        holdThis = true;
    }

    public char getNext()
    {
        try
        {
            if (holdThis)
            {
                holdThis = false;
            }
            else if (check())
            {
                bufferIdx++;
                currChar++;
                if ('\n' == buffer[bufferIdx] || '\r' == buffer[bufferIdx])
                {
                    currLine++;
                    currColumn = 0;
                }
                else
                {
                    currColumn++;
                }
            }
        }
        catch (Throwable lT)
        {
            Severity.DEATH.report(
                    this.toString(),
                    "reading",
                    "getNext(): length=" + length + " idx= " + bufferIdx + "; length - bufferIdx = " + (length - bufferIdx), lT);
        }
        return getThis();
    }

    public String getFileName()
    {
        return file.toURI().toString();
    }

    public int getCurrLineNum()
    {
        return currLine;
    }

    public int getCurrColumnNum()
    {
        return currColumn;
    }

    public int getCurrCharNum()
    {
        return currChar;
    }

    private boolean check()
    {
        if (done)
        {
            return false;
        }
        else if (bufferIdx + 1 >= length)
        {
            try
            {
                bufferIdx = -1;
                length = isr.read(buffer);
                done = (0 >= length);
            }
            catch (Throwable lT)
            {
                Severity.DEATH.report(file.getPath(), "reader creation", "could not allocate file input stream", lT);
            }
        }
        return -1 != bufferIdx || 0 < (length - bufferIdx);
    }

    private int getLength()
    {
        return length;
    }

    private char[] getBuffer()
    {
        return buffer;
    }

    private static FileInputStream fileInputStreamFactory(File aInFile)
    {
        try
        {
            return new FileInputStream(aInFile);
        }
        catch (Throwable lT)
        {
            Severity.DEATH.report(
                    aInFile.getPath(),
                    "reader creation",
                    "could not allocate file input stream", lT);
            return null;
        }
    }

    public String toString()
    {
        StringBuffer lSb = new StringBuffer();
        lSb.append("READER[");
        lSb.append(file.toURI());
        lSb.append('(');
        lSb.append(getCurrLineNum());
        lSb.append(':');
        lSb.append(getCurrColumnNum());
        lSb.append('/');
        lSb.append(getCurrCharNum());
        lSb.append(":bidx= ");
        lSb.append(bufferIdx);
        lSb.append(") DONE=");
        lSb.append(done);
        return lSb.toString();
    }

    private int currLine = 0;
    private int currColumn = 0;
    private int currChar = -1;
    private boolean holdThis = false;

    private int length = -1;
    private int bufferIdx = -1;
    private boolean done = false;
    private final File file;
    private final FileInputStream is;
    private final InputStreamReader isr;
    private final char[] buffer = new char[BUFF_SIZE];
}
