package org.opendaylight.opflex.genie.engine.file;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/23/14.
 */
public class WriteStats
{
    public synchronized void incrTotal()
    {
        ++total;
    }

    public synchronized void decrTotal()
    {
        --total;
    }

    public synchronized void incrNew()
    {
        ++newlyCreated;
    }

    public synchronized void decrNew()
    {
        --newlyCreated;
    }

    public synchronized void incrDuplicate()
    {
        ++duplicate;
    }

    public synchronized void decrDuplicate()
    {
        --duplicate;
    }

    public synchronized void incrChanged()
    {
        ++changed;
    }

    public synchronized void decrChanged()
    {
        --changed;
    }

    public void report()
    {
        Severity.INFO.report(
                "WRITE STATS","","","FILES WRITTEN:" +
                " total: " + total +
                " new: " + newlyCreated +
                " duplicate: " + duplicate +
                " changed: " + changed
                );
    }
    private int total = 0;
    private int newlyCreated = 0;
    private int duplicate = 0;
    private int changed = 0;
}
