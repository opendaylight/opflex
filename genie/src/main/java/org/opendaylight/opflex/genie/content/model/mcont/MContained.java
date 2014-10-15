package org.opendaylight.opflex.genie.content.model.mcont;

import java.util.Collection;
import java.util.LinkedList;
import java.util.Map;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.engine.model.*;

/**
 * Created by dvorkinista on 7/8/14.
 *
 * Specifies "contained" rule set for a given class. These rules are expressed from the vantage point of the
 * contained class on the managed information tree.
 *
 * Containment is a relationship between two classes. Each containment rule identifies that the child class can be
 * contained on the managed information tree by the parent class. In addition containment rule holds directives on
 * lifecycle control as well as directive affecting relative naming.
 *
 * CAT[mcont:mcontained]->MContained->MParent
 *
 * Containment relationships can't be instantiated via direct construction, use MContained.addRule(...) method.
 */
public class MContained
        extends MContItem
{
    /**
     * category identifying all contained rules
     */
    public static final Cat MY_CAT = Cat.getCreate("mcont:mcontained");

    /**
     * category identifying the target class of this relationship rule object
     */
    public static final RelatorCat TARGET_CAT = RelatorCat.getCreate("mcont:mcontainer:childref", Cardinality.SINGLE);

    /**
     * declares containment relationship.
     *
     * @param aInParentGName global name of the parent class
     * @param aInChildGName global name of the child class
     * @return pair of contained and container objects
     */
    public static final Pair<MContained, MParent> addRule(String aInParentGName, String aInChildGName)
    {
        MContained lContd = MContained.get(aInChildGName, true);
        MParent lMParent = lContd.getMParent(aInParentGName, true);

        MContainer lContr = MContainer.addRule(aInParentGName, aInChildGName);

        return new Pair<MContained, MParent>(lContd,lMParent);
    }

    /**
     * Constructor
     * @param aInChildGname global name of the contained object class
     */
    private MContained(String aInChildGname)
    {
        super(MY_CAT, null, TARGET_CAT, aInChildGname);
    }

    /**
     * Finds Contained rule corresponding to the global name of the class passed in.
     * If aInCreateIfNotFound is true and rule is not found, rule is created.
     * @param aInGName global name of the contained object class
     * @param aInCreateIfNotFound indicates that the rule needs to be created if not found.
     * @return contained rule associated with class identified by the global named passed in.
     */
    public static MContained get(String aInGName, boolean aInCreateIfNotFound)
    {
        MContained lContd = get(aInGName);
        if (null == lContd && aInCreateIfNotFound)
        {
            synchronized (MY_CAT)
            {
                lContd = get(aInGName);
                if (null == lContd)
                {
                    lContd = new MContained(aInGName);
                }
            }
        }
        return lContd;
    }

    /**
     * Finds Contained rule corresponding to the global name of the class passed in.
     * @param aInGName global name of the contained object class
     * @return contained rule associated with class identified by the global named passed in.
     */
    public static MContained get(String aInGName)
    {
        return (MContained) MY_CAT.getItem(aInGName);
    }

    /**
     * checks if there are per parent rules
     */
    public boolean hasParents()
    {
        return hasChildren(MParent.MY_CAT);
    }

    /**
     * retrieves set of rules, each specified per specific parent
     * @param aOut rules specified per specific parent
     */
    public void getParents(Collection<MParent> aOut)
    {
        LinkedList<Item> lItems = new LinkedList<Item>();
        getChildItems(MParent.MY_CAT, lItems);
        for (Item lIt : lItems)
        {
            aOut.add((MParent)lIt);
        }
    }

    /**
     * retrieves a rule specified per specific parent identified by corresponding class's global name
     * @param aInClassGName global name of the parent class
     */
    public MParent getMParent(String aInClassGName)
    {
        return (MParent) getChildItem(MParent.MY_CAT,aInClassGName);
    }

    /**
     * retrieves a rule specified per specific parent identified by corresponding class's global name
     * @param aInClassGName global name of the parent class
     * @param aInCreateIfNotFound specifies need of creation, if the rue is not found
     */
    public MParent getMParent(String aInClassGName, boolean aInCreateIfNotFound)
    {
        MParent lMParent = getMParent(aInClassGName);
        if (null == lMParent && aInCreateIfNotFound)
        {
            synchronized (this)
            {
                lMParent = getMParent(aInClassGName);
                if (null == lMParent)
                {
                    lMParent = new MParent(this, aInClassGName);
                }
            }
        }
        return lMParent;
    }

    /**
     * retrieves collection of classes that are potential parents
     * @param aOut collection of classes of potential parents
     */
    public void getParentClasses(Map<Ident,MClass> aOut, boolean aInResolveToConcrete)
    {
        LinkedList<Item> lItems = new LinkedList<Item>();
        getChildItems(MParent.MY_CAT, lItems);
        for (Item lIt : lItems)
        {
            MParent lParent = (MParent) lIt;
            MClass lThat = lParent.getTarget();
            if (aInResolveToConcrete && !lThat.isConcrete())
            {
                lThat.getSubclasses(aOut,false,aInResolveToConcrete);
            }
            else
            {
                aOut.put(lThat.getGID(), lThat);
            }
        }
    }
}
