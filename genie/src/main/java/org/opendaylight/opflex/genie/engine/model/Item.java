package org.opendaylight.opflex.genie.engine.model;

import java.util.Collection;
import java.util.LinkedList;

/**
 * Model item associated with corresponding model graph node
 *
 * Created by midvorki on 3/27/14.
 */
public class Item
        implements Validatable
{
    /**
     * Constructor. Constructs item and automatically attaches it to the graph.
     *
     * @param aInCat    category of the item and graph node
     * @param aInParent parent item
     * @param aInLName  local name within parent's context
     */
    protected Item(
            Cat aInCat,
            Item aInParent,
            String aInLName
            )
    {
        this(aInCat.nodeFactory(null == aInParent ? null : aInParent.getNode(), aInLName));
    }

    /**
     * Constructor. Constructs item and automatically attaches it to the graph.
     *
     * @param aInCat    category of the item and graph node
     * @param aInParent parent item
     * @param aInLId    local context identifier
     * @param aInGId    global context identifier
     */
    private Item(
            Cat aInCat,
            Item aInParent,
            Ident aInLId,
            Ident aInGId
            )
    {
        this(aInCat.nodeFactory(null == aInParent ? null : aInParent.getNode(), aInLId, aInGId));
    }

    /**
     * Constructor. Constructs item within context of passed in node.
     *
     * @param aInNode model graph node.
     */
    protected Item(Node aInNode)
    {
        node = aInNode;
        node.register(this);

        //System.out.println("ITEM::CONSTRUCTED(" + this + ")");
    }

    /**
     * Graph node accessor.
     *
     * @return corresponding model graph node
     */
    public Node getNode()
    {
        return node;
    }

    public Cat getCat()
    {
        return node.getCat();
    }

    /**
     * Global ID (gID) accessor
     *
     * @return global id
     */
    public Ident getGID()
    {
        return node.getGID();
    }

    /**
     * Local ID (lID) accessor
     *
     * @return local id
     */
    public Ident getLID()
    {
        return node.getLID();
    }

    public Item getParent()
    {
        return getNode().getParentItem();
    }

    public Item getAncestorOfCat(Cat aInCat)
    {
        return getNode().getAncestorItemOfCat(aInCat);
    }

    public boolean hasChildren()
    {
        return getNode().hasChildren();
    }

    public boolean hasChildren(Cat aIn)
    {
        return getNode().hasChildren(aIn);
    }

    public Children getChildren()
    {
        return getNode().getChildren();
    }

    public void getChildNodes(Cat aInCat, Collection<Node> aOut)
    {
        getNode().getChildNodes(aInCat,aOut);
    }

    public void getChildItems(Cat aInCat, Collection<Item> aOut)
    {
        getNode().getChildItem(aInCat, aOut);
    }

    public Item getChildItem(Cat aInCat, String aInName)
    {
        return getNode().getChildItem(aInCat,aInName);
    }

    public Node getChildNode(Cat aInCat, String aInName)
    {
        return getNode().getChildNode(aInCat, aInName);
    }

    /**
     * @Override
     */
    public void preValidateCb()
    {
    }

    /**
     * @Override
     */
    public void validateCb()
    {
    }

    /**
     * @Override
     */
    public void postValidateCb()
    {
    }

    /**
     * @Override
     */
    public void postLoadCb()
    {
    }

    public void metaModelLoadCompleteCb() {  }

    public void preLoadModelCompleteCb() {  }

    public void loadModelCompleteCb() {  }

    /**
     * Stringifier
     *
     * @return printable string of this item identity
     */
    public String toString()
    {
        StringBuilder lSb = new StringBuilder();
        lSb.append("item:");
        toIdentString(lSb);
        return lSb.toString();
    }

    public void toIdentString(StringBuilder aInSb)
    {
        if (null != node)
        {
            node.toIdentString(aInSb);
        }
        else
        {
            aInSb.append("unidentified: " + getClass().getName());
        }
    }

    /**
     * registers parsing data nodes that resulted in genesis and formation of this item
     * @param aIn parsing data node
     */
    public synchronized void addParsingDataNode(org.opendaylight.opflex.genie.engine.parse.modlan.Node aIn)
    {
        if (null == parsingDataNodes)
        {
            parsingDataNodes = new LinkedList<org.opendaylight.opflex.genie.engine.parse.modlan.Node>();
        }
        parsingDataNodes.add(aIn);
    }

    /**
     * checks if this item has parsing data nodes associated with it
     * @return true if this item has parsing data nodes
     */
    public boolean hasParsingDataNodes()
    {
        return null != parsingDataNodes && !parsingDataNodes.isEmpty();
    }

    /**
     * returns parsing data nodes associated with this item
     * @return parsing data nodes or null, if none are available
     */
    public LinkedList<org.opendaylight.opflex.genie.engine.parse.modlan.Node> getParsingDataNodes()
    {
        return parsingDataNodes;
    }

    /**
     * adds a list of comments to this item
     * @param aIn comments added
     */
    public void addComments(String[] aIn)
    {
        if (null == comments)
        {
            comments = new LinkedList<String>();
        }
        for (String lThis : aIn)
        {
            comments.add(lThis);
        }
    }

    /**
     * retrieves all comments associated with this item
     * @param aOut comments retrieved.
     */
    public void getComments(Collection<String> aOut)
    {
        if (hasParsingDataNodes())
        {
            for (org.opendaylight.opflex.genie.engine.parse.modlan.Node lNode : parsingDataNodes)
            {
                if (lNode.hasComments())
                {
                    aOut.addAll(lNode.getComments());
                }
            }
        }
        if (null != comments && !comments.isEmpty())
        {
            aOut.addAll(comments);
        }
    }


    /**
     * Corresponding model graph node.
     */
    protected final Node node;

    /**
     * parsing data nodes that resulted in genesis and formation of this item
     */
    private LinkedList<org.opendaylight.opflex.genie.engine.parse.modlan.Node> parsingDataNodes = null;

    private LinkedList<String> comments = null;
}
