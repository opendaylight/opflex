package org.opendaylight.opflex.genie.engine.format;

import java.util.TreeMap;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/24/14.
 */
public class FormatterFeatureMeta
{
    public FormatterFeatureMeta(String aInName)
    {
        name = aInName;
    }

    public String getName()
    {
        return name;
    }

    public boolean isEnabled()
    {
        return isEnabled;
    }

    public void setEnabled(boolean aIn)
    {
        isEnabled = aIn;
    }

    public void process(FormatterCtx aInCtx)
    {
        //System.out.println(this + ".process()");

        for (FormatterTaskMeta lTask : tasks.values())
        {
            if (lTask.isEnabled())
            {
                //System.out.println(this + ".process(): " + lTask);
                lTask.process(aInCtx);
            }
        }
    }

    public void addTask(FormatterTaskMeta aIn)
    {
        if (!tasks.containsKey(aIn.getName()))
        {
            tasks.put(aIn.getName(),aIn);
        }
        else
        {
            Severity.DEATH.report(toString(),"","", "task already registered: " + aIn.getName());
        }
    }

    public String toString()
    {
        return "formatter:feature(" + name + ')';
    }

    private final String name;
    private boolean isEnabled = true;
    private TreeMap<String,FormatterTaskMeta> tasks = new TreeMap<String,FormatterTaskMeta>();
}
