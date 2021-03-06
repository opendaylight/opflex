package org.opendaylight.opflex.genie.engine.model;

import java.util.LinkedList;
import java.util.Map;
import java.util.TreeMap;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Model Node Category. Specifies category role of the model Node.
 * For example, a property, a class, a type, a containment rule.
 *
 * Created by midvorki on 3/26/14.
 */
public class Cat extends Ident {
    protected Cat(String aInName)
    {
        super(allocateId(), aInName);
        nodes = new CatEntry(this, null, true);
        register(this);
    }

    public static synchronized Cat getCreate(String aIn)
    {
        Cat lCat = get(aIn);
        if (null == lCat)
        {
            lCat = new Cat(aIn);
        }
        return lCat;
    }

    /**
     * Looks up Cat entry by name
     * @param aIn : name of the category to be looked up
     * @return category corresponding to name passed in or null if it doesn't exist.
     */
    public static Cat get(String aIn)
    {
        return nameToCatTable.get(aIn);
    }

    /**
     * Adds model node to category registry
     * @param aIn model node to be added
     */
    public synchronized void addItem(Node aIn)
    {
        nodes.add(aIn);
    }

    public CatEntry getNodes()
    {
        return nodes;
    }

    public Item getItem(String aIn) { return nodes.getItem(aIn); }

    /**
     * Stringifier
     * @return
     */
    public String toString()
    {
        StringBuilder lSb = new StringBuilder();
        lSb.append("CAT:");
        toIdentString(lSb);
        return lSb.toString();
    }

    /**
     * Node factory. Constructs an instance of node. This function
     * can be overridden by subclasses.
     *
     * @param aInParent parent node
     * @param aInLName local name of the new node within parent's context
     * @return newly constructed node
     */
    public Node nodeFactory(Node aInParent, String aInLName)
    {
        Ident lLId = new Ident(allocateLocalId(aInParent, aInLName), aInLName);
        String lGName = allocateGlobalName(aInParent, aInLName);
        Ident lGId = new Ident(allocateGlobalId(aInParent, lGName, lLId.getId()), lGName);

        return nodeFactory(aInParent, lLId, lGId);
    }

    /**
     * Model graph node factory. Constructs an instance of node. This function
     * can be overridden by subclasses.
     *
     * @param aInParent parent node
     * @param aInLId local context identifier
     * @param aInGId global context identifier
     * @return newly constructed node
     */
    public Node nodeFactory(Node aInParent, Ident aInLId, Ident aInGId)
    {
        return new Node(aInParent, this, aInLId, aInGId);
    }

    private String allocateGlobalName(Node aInParent, String aInLName)
    {
        StringBuilder lSb = new StringBuilder();
        if (null != aInParent)
        {
            Ident lPGId = aInParent.getGID();
            if (null != lPGId)
            {
                lSb.append(lPGId.getName());
            }
            if (!lPGId.getName().endsWith("/"))
            {
                lSb.append('/');
            }
        }
        lSb.append(null != aInLName ? aInLName : getName());
        return lSb.toString();
    }

    private int allocateLocalId(Node aInParent, String aInLName)
    {
        if (null == aInParent)
        {
            // NO PARENT: LOCAL AND GLOBAL IDS ARE ALLOCATED FROM GLOBAL SCOPE WITHIN CATEGORY
            return nodes.allocateId('/' + aInLName);
        }
        else
        {
            return aInParent.allocateLocalId(this, aInLName);
        }
    }

    private int allocateGlobalId(Node aInParent, String aInGName, int aInLId)
    {
        if (null == aInParent)
        {
            return aInLId;
        }
        else
        {
            return nodes.allocateId(aInGName);
        }
    }


    /**
     * Category id allocator: allocates monotonically increasing ids.
     * @return newly allocated id
     */
    private static synchronized int allocateId()
    {
        return ++idAllocator;
    }

    /**
     * Category registration.
     * @param aIn category to be registered
     */
    private static synchronized void register(Cat aIn)
    {
        if (isValidated())
        {
            Severity.INFO.report(aIn.toString(),"register","","NOTHING TO PANIC ABOUT: register after validation");
        }
        if (null != nameToCatTable.put(aIn.getName(), aIn))
        {
            Severity.DEATH.report(aIn.toString(), "register", "duplicate", "name already defined; must be unique");
        }
        if (null != idToCatTable.put(aIn.getId(), aIn))
        {
            Severity.DEATH.report(aIn.toString(), "register", "duplicate", "id already defined; must be unique");
        }
    }

    public static void metaModelLoadComplete()
    {
        for (Cat lCat : new LinkedList<>(idToCatTable.values()))
        {
            lCat.metaModelLoadCompleteCb();
        }
    }

    public void metaModelLoadCompleteCb()
    {
        for (Node lNode : nodes.getList())
        {
            lNode.metaModelLoadCompleteCb();
        }
    }

    public static void preLoadModelComplete()
    {
        for (Cat lCat : new LinkedList<>(idToCatTable.values()))
        {
            lCat.preLoadModelCompleteCb();
        }
    }

    public void preLoadModelCompleteCb()
    {
        for (Node lNode : nodes.getList())
        {
            lNode.preLoadModelCompleteCb();
        }
    }

    public static void postLoad()
    {
        validated = true;

        for (Cat lCat : new LinkedList<>(idToCatTable.values()))
        {
            lCat.postLoadCb();
        }
    }

    public static void validateAll()
    {
        validate();
        postValidate();
    }

    private static void validate()
    {
        validated = true;

        for (Cat lCat : new LinkedList<>(idToCatTable.values()))
        {
            lCat.validateCb();
        }
    }

    private static void postValidate()
    {
        for (Cat lCat : new LinkedList<>(idToCatTable.values()))
        {
            lCat.postValidateCb();
        }
    }

    public void validateCb()
    {
        for (Node lNode : nodes.getList())
        {
            lNode.validateCb();
        }
    }

    public void postValidateCb()
    {
        for (Node lNode : nodes.getList())
        {
            lNode.postValidateCb();
        }
    }

    public void postLoadCb()
    {
        for (Node lNode : nodes.getList())
        {
            lNode.postLoadCb();
        }
    }

    public static boolean isValidated() { return validated; }

    private static boolean validated = false;
    private static int idAllocator = 0;
    private static final Map<String, Cat> nameToCatTable = new TreeMap<>();
    private static final Map<Number, Cat> idToCatTable = new TreeMap<>();
    private final CatEntry nodes;
}
