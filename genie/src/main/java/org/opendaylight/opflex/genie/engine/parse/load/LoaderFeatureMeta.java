package org.opendaylight.opflex.genie.engine.parse.load;

import java.util.TreeMap;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/25/14.
 */
public class LoaderFeatureMeta
{
    public LoaderFeatureMeta(String aInName)
    {
        name = aInName;
    }

    public String getName()
    {
        return name;
    }

    public void process(LoadStage aInStage)
    {
        //System.out.println(this + ".process()");

        for (LoaderIncludeMeta lTask : tasks.values())
        {
            //System.out.println(this + ".process(): " + lTask);
            lTask.process(aInStage);
        }
    }

    public void addInclude(LoaderIncludeMeta aIn)
    {
        if (!tasks.containsKey(aIn.getName()))
        {
            tasks.put(aIn.getName(),aIn);
        }
        else
        {
            Severity.DEATH.report(toString(),"","", "include already registered: " + aIn.getName());
        }
    }

    public String toString()
    {
        return "loader:feature(" + name + ')';
    }

    private final String name;
    private TreeMap<String,LoaderIncludeMeta> tasks = new TreeMap<String,LoaderIncludeMeta>();
}
