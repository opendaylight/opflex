package org.opendaylight.opflex.genie.engine.model;

import java.util.Collection;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Model graph node.
 *
 * Created by midvorki on 3/26/14.
 */
public class Node implements Validatable
{
    /**
     * Constructor
     *
     * @param aInParent parent node
     * @param aInCat    category of this node and corresponding item
     * @param aInLId    local identifier
     * @param aInGId    global identifier
     */
    public Node(
            Node aInParent, Cat aInCat, Ident aInLId, Ident aInGId
            )
    {
        parent = aInParent;
        cat = aInCat;
        lID = aInLId;
        gID = aInGId;
        cat.addItem(this);

        // now, attach the parent
        if (hasParent())
        {
            parent.attach(this);
        }
    }

    public Cat getCat()
    {
        return cat;
    }

    /**
     * Global ID (gID) accessor
     *
     * @return global id
     */
    public Ident getGID()
    {
        return gID;
    }

    /**
     * Local ID (lID) accessor
     *
     * @return local id
     */
    public Ident getLID()
    {
        return lID;
    }

    /**
     * determines if this node has parent
     * @return whether this node has a parent
     */
    public boolean hasParent()
    {
        return null != parent;
    }

    /**
     * Parent accessor
     * @return parent of this node, if this node has parent, otherwise null
     */
    public Node getParent()
    {
        return parent;
    }

    public Item getParentItem()
    {
        Node lParentNode = getParent();
        return null == lParentNode ? null : lParentNode.getItem();
    }


    public Node getAncestorOfCat(Cat aInCat)
    {
        Node lThat = this;

        while ((null != (lThat = lThat.getParent())) &&
               !lThat.getCat().equals(aInCat));

        return lThat;
    }

    public Node getAncestorOfNthDegree(int aInDegree)
    {
        Node lThat = null;
        for (lThat = getParent(); null != lThat; lThat = lThat.getParent())
        {
            if (0 == --aInDegree)
            {
                break;
            }
        }
        return lThat;
    }

    public Item getAncestorItemOfCat(Cat aInCat)
    {
        Node lAnc = getAncestorOfCat(aInCat);
        return null == lAnc ? null : lAnc.getItem();
    }

    public boolean hasChildren()
    {
        return null != children && !children.isEmpty();
    }

    public boolean hasChildren(Cat aIn)
    {
        if (null != children)
        {
            CatEntry lCE = children.getEntry(aIn);
            return !(null == lCE || lCE.isEmpty());
        }
        else
        {
            return false;
        }
    }

    public Children getChildren()
    {
        return getChildren(false);

    }

    public synchronized Children getChildren(boolean aInCreateIfDoesNotExist)
    {
        if (null == children && aInCreateIfDoesNotExist)
        {
            children = new Children(getGID().getName());
        }
        return children;
    }

    public Node getChildNode(Cat aInCat, String aInName)
    {
        return null == children ? null : children.getNode(aInCat, aInName);
    }

    public Item getChildItem(Cat aInCat, String aInName)
    {
        Node lNode = getChildNode(aInCat, aInName);
        return null == lNode ? null : lNode.getItem();
    }

    public void getChildNodes(Cat aInCat, Collection<Node> aOut)
    {
        if (null != children)
        {
            children.getNodes(aInCat, aOut);
        }
    }

    public void getChildItem(Cat aInCat, Collection<Item> aOut)
    {
        if (null != children)
        {
            children.getItem(aInCat, aOut);
        }
    }

    /**
     * registers item with this node
     * @param aIn
     */
    public void register(Item aIn)
    {
        if (null == item)
        {
            item = aIn;
        }
        else
        {
            Severity.DEATH.report(this.toString(), "resgister", "duplicate item", "item already registered");
        }
    }

    public Item getItem()
    {
        return item;
    }

    /**
     * @Override
     */
    public void preValidateCb()
    {
        getItem().preValidateCb();
    }

    /**
     * @Override
     */
    public void validateCb()
    {
        getItem().validateCb();
    }

    /**
     * @Override
     */
    public void postValidateCb()
    {
        getItem().postValidateCb();
    }

    /**
     * @Override
     */
    public void postLoadCb()
    {
        getItem().postLoadCb();
    }

    public void metaModelLoadCompleteCb() { getItem().metaModelLoadCompleteCb(); }

    public void preLoadModelCompleteCb() { getItem().preLoadModelCompleteCb(); }

    public void loadModelCompleteCb() { getItem().loadModelCompleteCb(); }

    /**
     * Stringifier
     * @return printable string of this Node identity
     */
    public String toString()
    {
        StringBuilder lSb = new StringBuilder();
        lSb.append("node:");

        toIdentString(lSb);

        return lSb.toString();
    }

    /**
     * Prints identity string.. Mostly for debug.
     * @param aIn
     */
    public void toIdentString(StringBuilder aIn)
    {
        if (null != cat)
        {
            cat.toIdentString(aIn);
        }
        else if (null != item)
        {
            aIn.append(item.getClass().getName());
        }
        else
        {
            aIn.append(getClass().getName());
        }
        aIn.append('(');
        if (null != gID)
        {
            aIn.append(gID.getName());
            /**
            aIn.append('(');
            aIn.append(gID.getId());
            if (null != lID)
            {
                aIn.append('|');
                aIn.append(lID.getId());
            }
            aIn.append(')');
             **/
        }
        else if (null != lID)
        {
            lID.toIdentString(aIn);
        }
        else
        {
            aIn.append("unidentified");
        }
        aIn.append(')');
    }

    /**
     * attaches a child node to this node
     * @param aInChildNode child node to be attached
     */
    private void attach(Node aInChildNode)
    {
        //System.out.println(this + ".attach(" + aInChildNode + ")");
        getChildren().add(aInChildNode);
    }

    /**
     * Local ID allocator for a named child in a given category
     * @param aInCat model category
     * @param aInLName local name
     * @return allocated scalar id
     */
    public int allocateLocalId(Cat aInCat, String aInLName)
    {
        return getChildren(true).allocateId(aInCat, aInLName);
    }

    /**
     * Node Category
     */
    private Cat cat;

    /**
     * Global identifier
     */
    private Ident gID;

    /**
     * Local identifier
     */
    private Ident lID;

    /**
     * Parent node
     */
    private Node parent;

    /**
     * This node's item item
     */
    private Item item = null;

    /**
     * child nodes of this node
     */
    private Children children = null;
}
