package org.opendaylight.opflex.genie.engine.model;

import java.util.Collection;
import java.util.Map;
import java.util.TreeMap;

/**
 * Tracker of child nodes of a node
 *
 * Created by midvorki on 3/27/14.
 */
public class Children
{
    /**
     * checks if there are no registered children in this tracker
     * @return true if there are no child nodes
     */
    public boolean isEmpty()
    {
        return null == children || children.isEmpty();
    }

    public Collection<CatEntry> getList()
    {
        return isEmpty() ? CatEntry.EMPTY_COL : children.values();
    }

    public Map<Cat, CatEntry> getMap()
    {
        return isEmpty() ? CatEntry.EMPTY_CAT_MAP : children;
    }

    /**
     * retrieves category tracker for passed in category
     * @param aIn model category
     * @return corresponding tracker
     */
    public CatEntry getEntry(Cat aIn)
    {
        return getEntry(aIn, false);
    }

    public Node getNode(Cat aInCat, String aInName)
    {
        CatEntry lEntry = getEntry(aInCat);
        return null == lEntry ? null : lEntry.get(aInName);
    }

    public Item getItem(Cat aInCat, String aInName)
    {
        Node lNode = getNode(aInCat, aInName);
        return null == lNode ? null : lNode.getItem();
    }

    public void getItem(Cat aInCat, Collection<Item> aOut)
    {
        CatEntry lEntry = getEntry(aInCat);
        if (null != lEntry)
        {
            lEntry.getItem(aOut);
        }
    }

    public void getNodes(Cat aInCat, Collection<Node> aOut)
    {
        CatEntry lEntry = getEntry(aInCat);
        if (null != lEntry)
        {
            lEntry.getNodes(aOut);
        }
    }

    /**
     * retrieves and optionally creates category tracker for passed in category
     * @param aIn model category
     * @param aInCreateIfDoesNotExist indicates whether to create a category tracker if one does not exist
     * @return category tracker
     */
    private synchronized CatEntry getEntry(Cat aIn, boolean aInCreateIfDoesNotExist)
    {
        if (null == children && aInCreateIfDoesNotExist)
        {
            children = new TreeMap<Cat, CatEntry>();
        }
        CatEntry lPerCat = children.get(aIn);
        if (null == lPerCat && aInCreateIfDoesNotExist)
        {
            lPerCat = new CatEntry(aIn, name, false);
            children.put(aIn, lPerCat);
        }
        //System.out.println(this + ".getEntry():" + lPerCat + " :: ALL CHILDREN: " + children.keySet());
        return lPerCat;
    }

    /**
     * registered passed in node to this tracker
     * @param aIn node to be added/registered
     */
    public void add(Node aIn)
    {
        getEntry(aIn.getCat(), true).add(aIn);
    }

    /**
     * scalar id allocator
     * @param aInCat category to which name corresponds
     * @param aInLName name for which ID is allocated
     * @return allocated id
     */
    public synchronized int allocateId(Cat aInCat, String aInLName)
    {
        return getEntry(aInCat, true).allocateId(aInLName);
    }

    public Children(String aInName)
    {
        name = aInName;
    }

    public String toString()
    {
        return "children[of " + name + "]";
    }

    private TreeMap<Cat, CatEntry> children = null;
    String name;
}
