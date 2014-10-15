package org.opendaylight.opflex.genie.engine.format;

import java.util.TreeMap;

import org.opendaylight.opflex.modlan.report.Severity;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/24/14.
 */
public class FormatterRegistry
{
    public static FormatterRegistry get()
    {
        return inst;
    }

    public void addDomain(FormatterDomainMeta aIn)
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

    public void process(FormatterCtx aInCtx)
    {
        if (Strings.isWildCard(aInCtx.getDomain()))
        {
            for (FormatterDomainMeta lDom : domains.values())
            {
                if (lDom.isEnabled())
                {
                    lDom.process(aInCtx);
                }
            }
        }
        else
        {
            FormatterDomainMeta lDom = domains.get(aInCtx.getDomain());
            if (null != lDom)
            {
                lDom.process(aInCtx);
            }
            else
            {
                Severity.DEATH.report(
                        toString(),"","", "domain not fund: " + aInCtx.getDomain() + "; domains: " + domains.keySet());
            }
        }
    }

    private FormatterRegistry() {}
    public String toString()
    {
        return "formatter:registry";
    }
    private TreeMap<String,FormatterDomainMeta> domains = new TreeMap<String,FormatterDomainMeta>();
    private static final FormatterRegistry inst = new FormatterRegistry();
}
