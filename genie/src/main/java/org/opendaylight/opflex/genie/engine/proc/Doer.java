package org.opendaylight.opflex.genie.engine.proc;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 3/26/14.
 */
public class Doer extends Thread
{
    public Doer(int aInId, Chnl aInChnl)
    {
        super("doer(" + aInId + ")");
        id = aInId;
        chnl = aInChnl;
    }

    public void run()
    {
        while (!chnl.isDeath())
        {
            Task lTask = chnl.poll();
            if (null != lTask)
            {
                try
                {
                    //Severity.INFO.report(toString(), "run", "task", "BEGIN: " + lTask);
                    lTask.run();
                    //Severity.INFO.report(toString(), "run", "task", "END:" + lTask);
                }
                catch(Throwable lT)
                {
                    Severity.DEATH.report(toString(),"run","exception eoncountered",lT);
                }
                finally
                {
                    chnl.doneCb();
                }
            }
        }
        Severity.INFO.report(toString(), "run", "task", "DEATH.");
    }

    public String toString()
    {
        return getName();
    }

    private final Chnl chnl;
    private final int id;
}
