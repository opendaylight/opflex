package org.opendaylight.opflex.modlan.report;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.PrintStream;

/**
 * Created by midvorki on 7/28/14.
 */
public class Reporter extends PrintStream
{

    public Reporter(File aIn) throws
                              FileNotFoundException
    {
        super(aIn);
    }

    public void print(boolean aIn) { super.print(printLineCnt(true, true, false) + aIn); }

    public void print(char aIn) { super.print(printLineCnt(true, true, false) + aIn); }

    public void print(int aIn) { super.print(printLineCnt(true, true, false) + aIn); }

    public void print(long aIn) { super.print(printLineCnt(true, true, false) + aIn); }

    public void print(float aIn) {super.print(printLineCnt(true, true, false) + aIn); }

    public void print(double aIn) { super.print(printLineCnt(true, true, false) + aIn); }

    public void print(char[] aIn) { super.print(printLineCnt(true, true, false) + new String(aIn)); }

    public void print(java.lang.String aIn) { super.print(printLineCnt(true, true, false) + aIn); }

    public void print(java.lang.Object aIn) { super.print(printLineCnt(true, true, false) + aIn); }

    public void println() { super.println(printLineCnt(true, true, false)); }

    public void println(boolean aIn) { super.println(printLineCnt(true, true, true) + aIn); }

    public void println(char aIn) { super.println(printLineCnt(true, true, true) + aIn); }

    public void println(int aIn) { super.println(printLineCnt(true, true, true) + aIn); }

    public void println(long aIn) { super.println(printLineCnt(true, true, true) + aIn); }

    public void println(float aIn) { super.println(printLineCnt(true, true, true) + aIn); }

    public void println(double aIn) { super.println(printLineCnt(true, true, true) + aIn); }

    public void println(char[] aIn) { super.println(printLineCnt(true, true, true) + new String(aIn)); }

    public void println(java.lang.String aIn) { super.println(printLineCnt(true, true, true) + aIn); }

    public void println(java.lang.Object aIn) { super.println(printLineCnt(true, true, true) + aIn); }

    public void report(String aIn)
    {
        super.println(aIn);
    }

    private String printLineCnt(boolean aInInclThread, boolean aInInclDefSev, boolean aInIsEndl)
    {
        if (isLastEndl(aInIsEndl))
        {
            StringBuilder lSb = new StringBuilder();
            lSb.append(getLinecnt());
            lSb.append(':');
            if (aInInclThread)
            {
                lSb.append(Thread.currentThread().getName());
                lSb.append('>');
            }
            if (aInInclDefSev)
            {
                lSb.append("DEBUG: ");
            }
            return lSb.toString();
        }
        else
        {
            return "";
        }

    }
    public synchronized int getLinecnt()
    {
        return ++linecnt;
    }

    private synchronized boolean isLastEndl(boolean aInIsEndl)
    {
        boolean lRet = lastEndl;
        lastEndl = aInIsEndl;
        return lRet;
    }

    private void resetLastEndl()
    {
        lastEndl = true;
    }
    private int linecnt = 0;
    private boolean lastEndl = true;
}
