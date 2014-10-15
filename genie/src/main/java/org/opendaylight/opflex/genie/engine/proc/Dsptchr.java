package org.opendaylight.opflex.genie.engine.proc;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 3/26/14.
 */
public class Dsptchr
{
    public Dsptchr(int aInSize)
    {
        size = aInSize;
        doers = new Doer[size];
        for (int i = 0; i < size; i++)
        {
            Doer lThis = new Doer(i + 1, chnl);
            doers[i] = lThis;
            Severity.INFO.report(lThis.toString(), "starting", "initialization", "doer " + (i + 1) + " started.");
            lThis.start();
        }
    }

    public void trigger(Task aInTask)
    {
        chnl.put(aInTask);
    }

    public void drain()
    {
        chnl.suspendUntilDrained();
    }

    public void kill()
    {
        chnl.markForDeath();
    }

    private int size;
    private Chnl chnl = new Chnl();
    private Doer doers[];
}
