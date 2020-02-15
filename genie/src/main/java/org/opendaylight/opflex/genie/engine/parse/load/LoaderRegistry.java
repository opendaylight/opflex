package org.opendaylight.opflex.genie.engine.parse.load;

import java.util.TreeMap;

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
    private final TreeMap<String,LoaderDomainMeta> domains = new TreeMap<>();
    private static final LoaderRegistry inst = new LoaderRegistry();
}
