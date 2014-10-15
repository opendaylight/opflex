package org.opendaylight.opflex.modlan.report;

import java.io.File;
import java.io.PrintStream;
import java.io.PrintWriter;

/**
 * Created by midvorki on 3/15/14.
 */
@SuppressWarnings("unused")
public enum Severity
{
    TODO("TODO:", false),
    INFO("INFO:", false),
    TRACE("INFO", false),
    WARN("WARN:", false),
    ERROR("ERR:", false),
    DEATH("DEATH:", true)
    ;

    Severity(String aInName, boolean aInAbort)
    {
        name = aInName;
        abort = aInAbort;
    }

    public void report(String aInContext, String aInAction, String aInCause, String aInM)
    {
        report(aInContext, aInAction, aInCause, aInM, null);
    }

    public void report(String aInContext, String aInAction, String aInCause, String aInM, Throwable aInT)
    {
        StringBuilder lSb = new StringBuilder();
        format(
              lSb,
              aInContext,
              aInAction,
              aInCause,
              aInM,
              null
              );

        doPrint(lSb.toString(),
                    null != aInT || (!abort) ?
                            aInT :
                            new Error(lSb.toString()));

        if (abort)
        {
            end(false);
        }
    }

    private void doPrint(String aInText, Throwable aInT)
    {
        switch (this)
        {
            case WARN:
            case ERROR:
            case DEATH:

                if (null == stream)
                {
                    System.err.println(aInText);
                    if (null != aInT)
                    {
                        aInT.printStackTrace();
                    }
                }
                else
                {
                    stream.report(aInText);
                    origErr.println(aInText);
                    if (null != aInT)
                    {
                        aInT.printStackTrace();
                        aInT.printStackTrace(origErr);
                    }

                }

                break;

            default:

                if (null == stream)
                {
                    System.err.println(aInText);
                    if (null != aInT)
                    {
                        aInT.printStackTrace();
                    }
                }
                else
                {
                    stream.report(aInText);
                    if (null != aInT)
                    {
                        aInT.printStackTrace();
                    }
                }
                break;
        }
    }
    public void report(String aInContext, String aInAction, String aInCause, Throwable aInT)
    {
        report(aInContext, aInAction, aInCause, null, aInT);
    }


    public void stack(String aInContext, String aInAction, String aInCause, String aInM)
    {
        stack(aInContext, aInAction, aInCause, aInM, null);
    }

    public void stack(String aInContext, String aInAction, String aInCause, String aInM, Throwable aInT)
    {
        StringBuilder lSb = new StringBuilder();
        format(
                lSb,
                aInContext,
                aInAction,
                aInCause,
                aInM,
                null);

        if (null != aInT)
        {
            aInT.printStackTrace();
            new Exception(lSb.toString(), aInT).printStackTrace();
        }
        else
        {
            new Exception(lSb.toString()).printStackTrace();
        }
    }

    public void stack(String aInContext, String aInAction, String aInCause, Throwable aInT)
    {
        stack(aInContext, aInAction, aInCause, null, aInT);
    }

    private void format(
            StringBuilder aOutSb,
            String aInContext,
            String aInAction,
            String aInCause,
            String aInM,
            Throwable aInT
            )
    {
        if (null == aInT && TRACE == this)
        {
            aInT = new Throwable();
        }

        int lThisCnt = (null == stream) ? 666 : stream.getLinecnt();

///        synchronized (this) { lThisCnt = count++; };

        aOutSb.append(lThisCnt);
        aOutSb.append(":" + Thread.currentThread().getName());
        aOutSb.append('>');

        aOutSb.append(name);
        if (null != aInContext && 0 < aInContext.length())
        {
            aOutSb.append(" CTX=[");
            aOutSb.append(aInContext);
            aOutSb.append(']');
        }
        if (null != aInAction && 0 < aInAction.length())
        {
            aOutSb.append(" ACT=[");
            aOutSb.append(aInAction);
            aOutSb.append(']');
        }
        if (null != aInCause && 0 < aInCause.length())
        {
            aOutSb.append(" CAUSE=[");
            aOutSb.append(aInCause);
            aOutSb.append(']');
        }
        if (null != aInM && 0 < aInM.length())
        {
            aOutSb.append(" MSG=[");
            aOutSb.append(aInM);
            aOutSb.append(']');
        }
        if (null != aInT)
        {
            aOutSb.append(" EXC=[");
            aOutSb.append(aInT.toString());
            if (null != aInT.getMessage() && 0 < aInT.getMessage().length())
            {
                if (null != aInM && 0 < aInM.length())
                {
                    aOutSb.append(" | ");
                }
                aOutSb.append(aInT.getMessage());
            }
            aOutSb.append(']');
            //aOutSb.append()
            StackTraceElement[] lTrace = aInT.getStackTrace();
            for (StackTraceElement lElem : lTrace)
            {
                aOutSb.append("\n    " + lElem.toString());
            }
        }
    }

    public static final void init(String aInReportRoot)
    {
        System.out.println("====================================================");
        System.out.println("==      STARTING GENIE, THE CODE WRITING ROBOT    ==");
        System.out.println("====================================================");

        try
        {

            File lDir = new File(aInReportRoot);
            if (!lDir.exists())
            {
                lDir.mkdirs();
            }
            File lLogFile = new File(aInReportRoot + "/GENLOG.txt");
            if (lLogFile.exists())
            {
                lLogFile.delete();
            }
            lLogFile.createNewFile();

            stream = new Reporter(lLogFile);
            System.out.println("LOG FILE: " + lLogFile.getAbsolutePath() );
            origOut = System.out;
            System.setOut(stream);
            origErr = System.err;
            System.setErr(stream);
        }
        catch (Throwable lT)
        {
            DEATH.report("log-initialization", "", "can't init stream", lT);
        }

    }

    public static final void end(boolean aInNormal)
    {
        ((null == origOut) ? System.out : origOut).println("====================================================");
        ((null == origOut) ? System.out : origOut).println("== GENIE THE ROBOT FINISHED WRITING CODE FOR YOU  ==");
        ((null == origOut) ? System.out : origOut).println("====================================================");
        if (null != stream)
        {
            stream.flush();
            stream.close();
        }
        if (!aInNormal)
        {
            System.exit(666);
        }
    }

    public String getName()
    {
        return name;
    }

    public boolean isAbort()
    {
        return abort;
    }

    private static PrintStream origOut = null;
    private static PrintStream origErr = null;
    private static Reporter stream = null;
    private String name;
    private boolean abort;
    private static int count = 0;
}
