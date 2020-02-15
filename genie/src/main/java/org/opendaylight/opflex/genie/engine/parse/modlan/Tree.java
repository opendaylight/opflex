package org.opendaylight.opflex.genie.engine.parse.modlan;

import java.util.LinkedList;

import org.opendaylight.opflex.modlan.parse.Consumer;
import org.opendaylight.opflex.modlan.parse.Ctx;
import org.opendaylight.opflex.modlan.parse.Engine;

/**
 * Created by midvorki on 3/15/14.
 */
public class Tree
        implements Consumer
{
    public Tree(ProcessorRegistry aInPreg)
    {
        preg = aInPreg;
    }

    private void process()
    {
        root.process(preg.getRoot());
    }

    public Engine getEngine()
    {
        return engine;
    }

    public void setEngine(Engine aIn)
    {
        engine = aIn;
    }
    //////////////////////////////////////////////////////////////
    // MODLAN PARSINNG CALLBACKS /////////////////////////////////
    //////////////////////////////////////////////////////////////

    public org.opendaylight.opflex.modlan.parse.Data onDocBegin(String aInName)
    {
        stack.push(root);
        return root;
    }
    public org.opendaylight.opflex.modlan.parse.Data onDocEnd(String aInName)
    {
        stack.pop();
        process();
        return root;
    }

    public org.opendaylight.opflex.modlan.parse.Data onNodeBegin(String aInName)
    {
        Node lData = new Node(stack.peek(), aInName);

        markContext(lData);

        stack.push(lData);
        lData.addComments(commentBuffer);
        commentBuffer.clear();
        return lData;
    }
    public org.opendaylight.opflex.modlan.parse.Data onNodeEnd(String aInName)
    {
        Node lData = stack.peek();
        lData.addComments(commentBuffer);
        commentBuffer.clear();
        stack.pop();
        return lData;
    }

    public org.opendaylight.opflex.modlan.parse.Data onQual(String aIn)
    {
        Node lData = stack.peek();
        lData.setQual(aIn);
        return lData;
    }

    public org.opendaylight.opflex.modlan.parse.Data onComment(String aInLine)
    {
        commentBuffer.add(aInLine);
        return null;
    }

    public org.opendaylight.opflex.modlan.parse.Data onText(String aInLine)
    {
        Node lData = stack.peek();
        lData.setValue(aInLine);
        return lData;
    }

    public org.opendaylight.opflex.modlan.parse.Data onValue(String aInValue)
    {
        Node lData = stack.peek();
        lData.setValue(aInValue);
        return lData;
    }

    public org.opendaylight.opflex.modlan.parse.Data onContentBegin(String aInName)
    {
        stack.peek().addComments(commentBuffer);
        commentBuffer.clear();
        return stack.peek();
    }

    public org.opendaylight.opflex.modlan.parse.Data onContentEnd(String aInName)
    {
        stack.peek().addComments(commentBuffer);
        commentBuffer.clear();
        return stack.peek();
    }

    private void markContext(Node aIn)
    {
        if (null != engine)
        {
            Ctx lCtx = engine.getCtx();
            if (null != lCtx)
            {
                aIn.setLineNum(lCtx.getCurrLineNum());
                aIn.setColumnNum(lCtx.getCurrColumnNum());
                aIn.setFileName(lCtx.getFileName());
            }
        }
    }

    //////////////////////////////////////////////////////////////
    // PRIVATE DATA //////////////////////////////////////////////
    //////////////////////////////////////////////////////////////


    private final java.util.Stack<Node> stack = new java.util.Stack<>();
    private final Node root = new Node(null, "doc-root");
    private final LinkedList<String> commentBuffer = new LinkedList<>();
    private final ProcessorRegistry preg;
    private Engine engine = null;
}
