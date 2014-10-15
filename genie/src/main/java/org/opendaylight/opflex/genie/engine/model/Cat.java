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
public class Cat extends Ident implements Validatable
{
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
     * Looks up Cat entry by name
     * @param aIn : name of the category to be looked up
     * @return category corresponding to name passed in or null if it doesn't exist.
     */
    public static Cat get(int aIn)
    {
        return idToCatTable.get(aIn);
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

    public Node getNode(String aIn) { return nodes.get(aIn); }
    public Node getNode(int aIn) { return nodes.get(aIn); }
    public Item getItem(String aIn) { return nodes.getItem(aIn); }
    public Item getItem(int aIn) { return nodes.getItem(aIn); }

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
        //Severity.INFO.report("CAT", "metaModelLoadComplete", "metaModelLoadComplete", "all");

        for (Cat lCat : new LinkedList<Cat>(idToCatTable.values()))
        {
            lCat.metaModelLoadCompleteCb();
            //Severity.INFO.report(lCat.toString(), "metaModelLoadComplete", "metaModelLoadComplete", "DONE");
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
        //Severity.INFO.report("CAT", "preLoadModelComplete", "preLoadModelComplete", "all");

        for (Cat lCat : new LinkedList<Cat>(idToCatTable.values()))
        {
            lCat.preLoadModelCompleteCb();
            //Severity.INFO.report(lCat.toString(), "preLoadModelComplete", "preLoadModelComplete", "DONE");
        }
    }

    public void preLoadModelCompleteCb()
    {
        for (Node lNode : nodes.getList())
        {
            lNode.preLoadModelCompleteCb();
        }
    }

    public static void loadModelComplete()
    {
        //Severity.INFO.report("CAT", "loadModelComplete", "loadModelComplete", "all");

        for (Cat lCat : new LinkedList<Cat>(idToCatTable.values()))
        {
            lCat.loadModelCompleteCb();
            //Severity.INFO.report(lCat.toString(), "loadModelComplete", "loadModelComplete", "DONE");
        }
    }

    public void loadModelCompleteCb()
    {
        for (Node lNode : nodes.getList())
        {
            lNode.loadModelCompleteCb();
        }
    }


    public static void postLoad()
    {
        //Severity.INFO.report("CAT", "postLoad", "postLoad", "all");
        validated = true;

        for (Cat lCat : new LinkedList<Cat>(idToCatTable.values()))
        {
            lCat.postLoadCb();
            //Severity.INFO.report(lCat.toString(), "postLoad", "postLoad", "DONE");
        }
    }

    public static void validateAll()
    {
        preValidate();
        validate();
        postValidate();
    }


    private static void preValidate()
    {
        //Severity.INFO.report("CAT", "preValidate", "pre-validatin", "all");

        for (Cat lCat : new LinkedList<Cat>(idToCatTable.values()))
        {
            lCat.preValidateCb();
            //Severity.INFO.report("CAT", "preValidate", "pre-validatin", lCat.toString());

        }
    }

    private static void validate()
    {
        //Severity.INFO.report("CAT", "validate", "validatin", "all");
        validated = true;

        for (Cat lCat : new LinkedList<Cat>(idToCatTable.values()))
        {
            //Severity.INFO.report("CAT", "validate", "validating", lCat.toString());
            lCat.validateCb();
            //Severity.INFO.report("CAT", "validate", "validatin", lCat.toString());
        }
    }

    private static void postValidate()
    {
        //Severity.INFO.report("CAT", "postValidate", "post-validatin", "all");

        for (Cat lCat : new LinkedList<Cat>(idToCatTable.values()))
        {
            lCat.postValidateCb();
            //Severity.INFO.report("CAT", "postValidate", "post-validatin", lCat.toString());
        }
    }

    /**
     * @Override
     */
    public void preValidateCb()
    {
        for (Node lNode : nodes.getList())
        {
            lNode.preValidateCb();
        }
    }

    public void validateCb()
    {
        for (Node lNode : nodes.getList())
        {
            lNode.validateCb();
        }
    }

    /**
     * @Override
     */
    public void postValidateCb()
    {
        for (Node lNode : nodes.getList())
        {
            lNode.postValidateCb();
        }
    }

    /**
     * @Override
     */
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
    private static Map<String, Cat> nameToCatTable = new TreeMap<String, Cat>();
    private static Map<Number, Cat> idToCatTable = new TreeMap<Number, Cat>();
    private final CatEntry nodes;
}
