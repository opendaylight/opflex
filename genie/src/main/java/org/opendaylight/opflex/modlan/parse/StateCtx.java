package org.opendaylight.opflex.modlan.parse;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 3/16/14.
 */
public class StateCtx
{
    public StateCtx(State aInState, String aInText, StateCtx aInParent)
    {
        text = aInText;
        state = aInState;
        parent = aInParent;
        synchronized (StateCtx.class) { id = ++COUNT; }
        depth = (null == aInParent) ? 0 : aInParent.getDepth() + 1;
        //System.out.println("******* NEW : " + this);
    }

    public String toString()
    {
        StringBuilder lSB = new StringBuilder();
        lSB.append("StateCtx(state=");
        lSB.append(state);
        lSB.append(", id=");
        lSB.append(id);
        lSB.append(", d=");
        lSB.append(depth);
        lSB.append(", p=");
        StateCtx lThis = this;
        while (null != lThis)
        {
            lSB.append('/');
            if (null != lThis.text &&
                0 < lThis.text.length())
            {
                lSB.append(lThis.text);
            }
            else
            {
                lSB.append(lThis.state);
            }
            lThis = lThis.parent;
        }
        lSB.append(')');

        return lSB.toString();
    }

    public State getState()
    {
        return state;
    }

    public String getText()
    {
        return text;
    }

    public Object getParent()
    {
        return parent;
    }

    public int getId()
    {
        return id;
    }

    public Data getData()
    {
        return data;
    }

    public void setData(Data aIn)
    {
        if (null != aIn)
        {
            if (null == data)
            {
                data = aIn;
            }
            else if (data != aIn)
            {
                Severity.DEATH.report("DATA", "parse", "setData", "data inconsistency");
            }
        }
    }

    public int getDepth()
    {
        return depth;
    }

    private final String text;
    private final State state;
    private final StateCtx parent;
    private int id;
    private int depth = 0;
    private Data data = null;
    private static int COUNT = 0;
}
