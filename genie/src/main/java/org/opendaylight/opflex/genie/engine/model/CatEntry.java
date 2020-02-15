package org.opendaylight.opflex.genie.engine.model;

import java.util.Collection;
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
    public static final Map<Cat, CatEntry> EMPTY_CAT_MAP = new TreeMap<>();

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

    public boolean isEmpty()
    {
        return byName.isEmpty();
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
        Map<Ident,Item> lMap = new TreeMap<>();
        getItems(lMap);
        return lMap.values();
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

    private final TreeMap<String, Node> byName = new TreeMap<>();
    private final TreeMap<Number, Node> byId = new TreeMap<>();
    private final TreeMap<String, Number> ids = new TreeMap<>();

    private final String name;
    private final Cat cat;
    private final boolean global;
}