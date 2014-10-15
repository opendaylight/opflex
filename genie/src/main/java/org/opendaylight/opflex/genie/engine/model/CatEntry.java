package org.opendaylight.opflex.genie.engine.model;

import java.util.Collection;
import java.util.LinkedList;
import java.util.Map;
import java.util.TreeMap;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Per category collection of nodes and id tracking class.
 * Can be applied to local or global tracking.
 *
 * Created by midvorki on 3/27/14.
 */
public class CatEntry
{
    public static final Collection<CatEntry> EMPTY_COL = new LinkedList<CatEntry>();
    public static final Map<Cat, CatEntry> EMPTY_CAT_MAP = new TreeMap<Cat, CatEntry>();

    /**
     * Constructor
     * @param aInCat category for which item is tracked
     * @param aInIsGlobal indicates whether tracking is global or local
     */
    public CatEntry(Cat aInCat, String aInName, boolean aInIsGlobal)
    {
        name = aInName;
        cat = aInCat;
        global = aInIsGlobal;
    }

    /**
     * category accessor
     * @return category to which this node tracker corresponds to
     */
    public Cat getCat()
    {
        return cat;
    }

    /**
     * indicates whether tracking performed is global or local
     * @return if this tracker is global
     */
    public boolean isGlobal()
    {
        return global;
    }

    /**
     * retrieves tracked node by name
     * @param aInName name of the node
     * @return model node corresponding to the name in the category context
     */
    public Node get(String aInName)
    {
        return byName.get(aInName);
    }

    /**
     * retrieves tracked node by id
     * @param aInId id of the node
     * @return model node corresponding to the id in the category context
     */
    public Node get(int aInId)
    {
        return byName.get(aInId);
    }

    public boolean isEmpty()
    {
        return null == byName || byName.isEmpty();
    }

    public void getNodes(Collection<Node> aOut)
    {
        aOut.addAll(byName.values());
    }

    public void getItem(Collection<Item> aOut)
    {
        for (Node lNode : byName.values())
        {
            aOut.add(lNode.getItem());
        }
    }

    public Item getItem(String aInName)
    {
        Node lNode = get(aInName);
        return null == lNode ? null : lNode.getItem();
    }

    public Item getItem(int aInId)
    {
        Node lNode = get(aInId);
        return null == lNode ? null : lNode.getItem();
    }

    /**
     * lists the nodes tracked by this tracker
     * @return list of nodes tracked
     */
    public Collection<Node> getList()
    {
        return byId.values();
    }

    /**
     * lists the items tracked by this tracker
     * @param aOut  map of items tracked
     */
    public void getItems(Map<Ident,Item> aOut)
    {
         for (Node lNode : byId.values())
         {
             aOut.put(lNode.getGID(),lNode.getItem());
         }
    }

    /**
     * lists the items tracked by this tracker
     * @return aOut  map of items tracked
     */
    public Collection<Item> getItemsList()
    {
        Map<Ident,Item> lMap = new TreeMap<Ident,Item>();
        getItems(lMap);
        return lMap.values();
    }


    /**
     * maps tracked nodes by name
     * @return map of nodes keyed by name
     */
    public Map<String, Node> getByName()
    {
        return byName;
    }

    /**
     * maps tracked nodes by id
     * @return map of nodes by id
     */
    public Map<Number, Node> getById()
    {
        return byId;
    }

    /**
     * scalar id allocator
     * @param aIn name for which ID is allocated
     * @return id
     */
    public synchronized int allocateId(String aIn)
    {
        int lRet = ids.size() + 1;
        if (null != ids.put(aIn, lRet))
        {
            Severity.DEATH.report(
                    aIn,
                    "id allocation", "id allocated",
                    "local name " + aIn + " already exists in context for category " + cat +
                    "\n:::: " + byId);
        }
        return lRet;
    }

    /**
     * registered passed in node to this tracker
     * @param aIn node to be added/registered
     */
    public synchronized void add(Node aIn)
    {
        //System.out.println(this + ".add(" + aIn + ")");
        if (null != byName.put(
                        global ? aIn.getGID().getName() :
                                 aIn.getLID().getName(),
                        aIn))
        {
            Severity.DEATH.report(
                    aIn.toString(),
                    "add", "can't add",
                    (global ? aIn.getGID().getName() :
                              aIn.getLID().getName()) +
                    " already exists in context for category " + cat);
        }
        if (null != byId.put(
                        global ? aIn.getGID().getId() :
                                 aIn.getLID().getId(),
                        aIn))
        {
            Severity.DEATH.report(
                    aIn.toString(),
                    "add", "can't add",
                    (global ? aIn.getGID().getId() :
                              aIn.getLID().getId()) +
                    " already exists in context for category " + cat);
        }
    }

    public String toString()
    {
        return (global ? "global-" : "local-") + "cat-entry(" + cat.getName() + (null == name ? "" : (" of " + name)) + ")";
    }

    private TreeMap<String, Node> byName = new TreeMap<String, Node>();
    private TreeMap<Number, Node> byId = new TreeMap<Number, Node>();
    private TreeMap<String, Number> ids = new TreeMap<String, Number>();

    private final String name;
    private final Cat cat;
    private final boolean global;
    //private final int id = ++ID_GEN;
    //private static int ID_GEN = 0;
}