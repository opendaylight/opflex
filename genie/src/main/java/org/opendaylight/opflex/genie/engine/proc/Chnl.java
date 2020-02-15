package org.opendaylight.opflex.genie.engine.proc;

import java.util.LinkedList;
import java.util.Queue;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 3/26/14.
 */
public class Chnl
{
    public synchronized void doneCb()
    {
        if (0 == --procCount &&
            Status.SUSPEND == status)
        {
            status = Status.RUNNING;
            notifyAll();
        }
    }

    public Task poll()
    {
        Task lTask;

        synchronized (this)
        {
            try
            {
                while (null == (lTask = queue.poll()))
                {
                    if (isDeath())
                    {
                        return null;
                    }
                    try
                    {
                        wait();
                    }
                    catch (InterruptedException lE)
                    {
                    }
                }
            }
            finally
            {
                notifyAll();
            }
        }
        return lTask;
    }

    public void suspendUntilDrained()
    {
        synchronized (this)
        {
            if (!isDeath())
            {
                while (true)
                {
                    if (hasOutstandingTasks())
                    {
                        status = Status.SUSPEND;
                        try
                        {
                            wait();
                        }
                        catch (InterruptedException lE)
                        {
                        }
                    }
                    else
                    {
                        notifyAll();
                        return;
                    }
                }
            }
            else
            {
                notifyAll();
            }
        }
    }

    public boolean isDeath()
    {
        synchronized (this)
        {
            return Status.DEATH == status;
        }
    }

    private boolean hasOutstandingTasks()
    {
        return (!queue.isEmpty()) || (0 < procCount);
    }

    public void markForDeath()
    {
        synchronized (this)
        {
            while (hasOutstandingTasks())
            {
                Severity.INFO.report("processor:chnl", "markForDeath", "", "WAITING FOR QUEUE TO DRAIN???????????");

                try
                {
                    wait();
                }
                catch (InterruptedException lE)
                {
                }
            }
            status = Status.DEATH;
            notifyAll();
        }
    }

    public void put(Task aInTask)
    {
        synchronized (this)
        {
            if (!isDeath())
            {
                procCount++;

                while (Status.SUSPEND == status && (!(Thread.currentThread() instanceof Doer)))
                {
                    try
                    {
                        wait();
                    }
                    catch (InterruptedException lE)
                    {
                    }
                }
                queue.add(aInTask);
                notifyAll();
            }
            else
            {
                notifyAll();
            }
        }
    }

    private int procCount = 0;
    private final Queue<Task> queue = new LinkedList<>();
    private Status status = Status.RUNNING;
}
