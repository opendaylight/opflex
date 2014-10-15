package org.opendaylight.opflex.genie.engine.parse.load;

import java.util.TreeMap;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/25/14.
 */
public class LoaderRegistry
{
    public static LoaderRegistry get()
    {
        return inst;
    }

    public LoaderDomainMeta getDomain(String aInName, boolean aInCreateIfNotFound)
    {
        LoaderDomainMeta lDom = domains.get(aInName);
        if (null == lDom && aInCreateIfNotFound)
        {
            lDom = new LoaderDomainMeta(aInName);
            domains.put(aInName,lDom);
        }
        return lDom;
    }

    public void addDomain(LoaderDomainMeta aIn)
    {
        if (!domains.containsKey(aIn.getName()))
        {
            domains.put(aIn.getName(),aIn);
        }
        else
        {
            Severity.DEATH.report(toString(),"","", "domain already registered: " + aIn.getName());
        }
    }

    public void process(LoadStage aInStage)
    {
        for (LoaderDomainMeta lThis : domains.values())
        {
            lThis.process(aInStage);
        }
    }

    public String toString()
    {
        return "loader:registry";
    }
    private TreeMap<String,LoaderDomainMeta> domains = new TreeMap<String,LoaderDomainMeta>();
    private static final LoaderRegistry inst = new LoaderRegistry();
}
