package org.opendaylight.opflex.genie.engine.parse.model;

import java.util.TreeMap;

import org.opendaylight.opflex.genie.engine.parse.modlan.Processor;
import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 3/22/14.
 */
public abstract class ProcessorNode
        implements Processor
{
    /////////////////////////////////////////////
    // PROCESSING METHODS
    /////////////////////////////////////////////

    public String getName()
    {
        return name;
    }

    public ProcessorNode getChild(String aInName)
    {
        ProcessorNode lThis = null;

        if (null == children)
        {
            lThis = getRecursion();
        }
        else
        {
            lThis = children.get(aInName);
            if (null == lThis)
            {
                lThis = getRecursion();
            }
        }
        if (null == lThis)
        {
            Severity.INFO.report(this.toString(),"child retrieval", "child \"" + aInName + "\" doesn't exist", "can't find child " + aInName);
        }
        return lThis;
    }

    private ProcessorNode getRecursion()
    {
        /**
        ProcessorNode lThis = null;
        if (isRecursive)
        {
            for (lThis = this;
                 null != lThis && lThis.isRecursive && !lThis.equals(getName());
                 lThis = lThis.parent)
            {

            }
        }
         **/
        return isRecursive ? this : null;
    }

    /////////////////////////////////////////////
    // REGISTRATION AND HOUSEKEEPING
    /////////////////////////////////////////////

    public void addChild(ProcessorNode aInNode)
    {
        if (null == children)
        {
            children = new TreeMap<String, ProcessorNode>();
        }
        children.put(aInNode.getName(), aInNode);
        aInNode.addParent(this);
    }

    protected void addParent(ProcessorNode aInParent)
    {
        if (null != parent)
        {
            Severity.DEATH.report(
                this.toString(),
                "add child processing node",
                "redundant attachment", "already attached to " + parent);
        }
        parent = aInParent;
    }

    public ProcessorNode getParent()
    {
        return parent;
    }

    protected ProcessorNode(String aInName, boolean aInIsRecursive)
    {
        name = aInName;
        isRecursive = aInIsRecursive;
    }

    public String toString()
    {
        ProcessorNode lThis = this;
        StringBuilder lSb = new StringBuilder();
        lSb.append("parse-proc-node[");
        while (null != lThis)
        {
            lSb.append('/');
            lSb.append(lThis.name);
            lThis = lThis.parent;
        }
        lSb.append(']');
        return lSb.toString();
    }

    private String name;
    private boolean isRecursive;
    private TreeMap<String, ProcessorNode> children = null;
    private ProcessorNode parent = null;
}
