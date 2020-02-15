package org.opendaylight.opflex.genie.engine.proc;

/**
 * Created by midvorki on 3/26/14.
 */
public class Dsptchr
{
    public Dsptchr()
    {
        Doer lThis = new Doer(1, chnl);
        lThis.start();
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

    private final Chnl chnl = new Chnl();
}
