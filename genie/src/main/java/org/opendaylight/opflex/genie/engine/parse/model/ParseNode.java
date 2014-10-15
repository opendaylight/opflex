package org.opendaylight.opflex.genie.engine.parse.model;

import java.util.Collection;
import java.util.Map;
import java.util.TreeMap;

import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ProcessorNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/16/14.
 */
public class ParseNode extends ProcessorNode
{
    public ParseNode(String aInName)
    {
        this(aInName, false);
    }

    public ParseNode(String aInName, boolean aInIsRecursive)
    {
        super(aInName,aInIsRecursive);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        return null; // ASSUME DEFAULTS, AS NOTHING DONE
    }

    public void endCB(Node aInData, Item aInItem)
    {
        // DO NOTHING
    }

    public boolean hasProps()
    {
        return null != props && !props.isEmpty();
    }

    public Collection<String> getPropNames()
    {
        return hasProps() ? props.keySet() : null;
    }

    public void addProp(ParseNodeProp aInProp)
    {
        if (null == props)
        {
            props = new TreeMap<String, ParseNodeProp>();
        }

        if (!props.containsKey(aInProp.getName()))
        {
            props.put(aInProp.getName(),aInProp);
        }
        else
        {
            Severity.DEATH.report(this.toString(), "add parsing prop", "", aInProp + " already registered;");
        }
    }

    public ParseNodeProp getProp(String aInName)
    {
        return hasProps() ? props.get(aInName) : null;
    }

    public boolean hasProp(String aInName)
    {
        return hasProps() && props.containsKey(aInName);
    }

    private Map<String,ParseNodeProp> props = null;
}
