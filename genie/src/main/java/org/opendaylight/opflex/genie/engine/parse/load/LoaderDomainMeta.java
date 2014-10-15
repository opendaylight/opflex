package org.opendaylight.opflex.genie.engine.parse.load;

import java.util.TreeMap;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/25/14.
 */
public class LoaderDomainMeta
{
    public LoaderDomainMeta(String aInName)
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

    public void process(LoadStage aInStage)
    {
        //System.out.println(this + ".ptocess(" + aInStage + ")");
        for (LoaderFeatureMeta lTask : features.values())
        {
            lTask.process(aInStage);
        }
    }

    public LoaderFeatureMeta getFeature(String aInName, boolean aInCreateIfNotFound)
    {
        LoaderFeatureMeta lF = features.get(aInName);
        if (null == lF && aInCreateIfNotFound)
        {
            lF = new LoaderFeatureMeta(aInName);
            features.put(aInName,lF);
        }
        return lF;
    }

    public void addFeature(LoaderFeatureMeta aIn)
    {
        if (!features.containsKey(aIn.getName()))
        {
            features.put(aIn.getName(),aIn);
        }
        else
        {
            Severity.DEATH.report(toString(),"","", "task already registered: " + aIn.getName());
        }
    }

    public String toString()
    {
        return "loader:domain(" + name + ')';
    }

    private final String name;
    private boolean isEnabled = true;
    private TreeMap<String,LoaderFeatureMeta> features = new TreeMap<String,LoaderFeatureMeta>();
}
