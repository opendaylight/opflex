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
            //Severity.INFO.report("processor:chnl", "doneCb", "", "UNSUSPENDING: ALL TASKS DONE");
            status = Status.RUNNING;
            notifyAll();
        }
    }

    public Task poll()
    {
        Task lTask = null;

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
            /**
            if (null != lTask)
            {
                procCount++;
            }
             **/
        }
        return lTask;
    }

    /*
    public synchronized void suspendUntilDrained()
    {
        if (!isDeath())
        {
            if (hasOutstandingTasks())
            {
                Severity.INFO.report("processor:chnl", "suspend", "", "SUSPENDING CHNL!!");
                status = Status.SUSPEND;
            }
            else
            {
                Severity.INFO.report("processor:chnl", "suspend", "", "NO TASKS: NO SUSPENSION NECESSARY!!");
            }
        }
        else
        {
            notifyAll();
        }
    }
    */
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
                        //Severity.INFO.report("processor:chnl", "suspend", "", "SUSPENDING CHNL!!");
                        status = Status.SUSPEND;
                        try
                        {
                            wait();
                        }
                        catch (InterruptedException lE)
                        {
                        }
                        //Severity.INFO.report("processor:chnl", "suspend", "", "WAKE UP!!");
                    }
                    else
                    {
                        //Severity.INFO.report("processor:chnl", "suspend", "", "NO TASKS: NO SUSPENSION NECESSARY!!");
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
            //Severity.INFO.report(
            //                "processor:chnl", "markForDeath", "", "WILL MARK FOR DEATH.................");

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

            //Severity.INFO.report("processor:chnl", "markForDeath", "", "MARKED FOR DEATH!!!!!!!!!!!!!!!!!!!");

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
                    //Severity.INFO.report("processor:chnl", "put", "task", "SUSPENDED: BLOCKING!");

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
    private Queue<Task> queue = new LinkedList<Task>();
    private Status status = Status.RUNNING;

}
