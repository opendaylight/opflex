package org.opendaylight.opflex.genie.content.model.mcont;

import java.util.Collection;
import java.util.LinkedList;
import java.util.Map;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.engine.model.*;

/**
 * Created by dvorkinista on 7/8/14.
 *
 * Specifies "container" rule set for a given class. These rules are expressed from the vantage point of the
 * container class on the managed information tree.
 *
 * Containment is a relationship between two classes. Each containment rule identifies that the child class can be
 * contained on the managed information tree by the parent class. In addition containment rule holds directives on
 * lifecycle control as well as directive affecting relative naming.
 *
 * CAT[mcont:mcontainer]->MContainer->MChild
 *
 * Containment relationships can't be instantiated via direct construction, use MContained.addRule(...) method.
 */
public class MContainer
        extends MContItem
{
    /**
     * Container category definition. Contains registry of all container definitions.
     */
    public static final Cat MY_CAT = Cat.getCreate("mcont:mcontainer");

    /**
     * Container target class category definition. Contains references between containers and their represented classes.
     */
    public static final RelatorCat TARGET_CAT = RelatorCat.getCreate("mcont:mcontainer:parentref", Cardinality.SINGLE);

    /**
     * rule registration API. Adds or finds containment rule. This API is scoped to the package and is for internal use.
     * @param aInParentGName name of the parent class
     * @param aInChildGName name of the child class
     * @return container corresponding to the parent name passed in.
     */
    static final MContainer addRule(String aInParentGName, String aInChildGName)
    {
        MContainer lContr = MContainer.get(aInParentGName, true);
        lContr.getMChild(aInChildGName, true);

        return lContr;
    }

    /**
     * Constructor.
     * @param aInParentGname name of the the class for whom the container rule is allocated
     */
    MContainer(String aInParentGname)
    {
        super(MY_CAT, null, TARGET_CAT, aInParentGname);
    }

    /**
     * Container instance finder
     * @param aInGName name of the class for which "container" is to be found
     * @return container associated with the class corresponding to the name passed in.
     */
    public static MContainer get(String aInGName)
    {
        return (MContainer) MY_CAT.getItem(aInGName);
    }

    /**
     * retrieves or creates a container. This is an internal API used within te package.
     * @param aInGName name of the class for which container is resolved.
     * @param aInCreateIfNotFound specifies whether container needs to be created if not found
     * @return container associated with a class corresponding to the name passed in
     */
    static synchronized MContainer get(String aInGName, boolean aInCreateIfNotFound)
    {
        MContainer lContr = get(aInGName);
        if (null == lContr && aInCreateIfNotFound)
        {
            lContr = new MContainer(aInGName);
        }
        return lContr;
    }

    /**
     * checks if there are per child rules per this container
     * @return true if this container has children containment rules.
     */
    public boolean hasChild()
    {
        return hasChildren(MChild.MY_CAT);
    }

    /**
     * retrieves child containment rules specified within this container.
     * @param aOut set of child containement rules
     */
    public void getChildren(Collection<MChild> aOut)
    {
        LinkedList<Item> lItems = new LinkedList<Item>();
        getChildItems(MChild.MY_CAT, lItems);
        for (Item lIt : lItems)
        {
            aOut.add((MChild)lIt);
        }
    }

    /**
     * retrieves specific child containment rule corresponding to the class with a name passed in
     * @param aInClassGName global name of the associated contained class
     * @return child containment rule
     */
    public MChild getMChild(String aInClassGName)
    {
        return (MChild) getChildItem(MChild.MY_CAT,aInClassGName);
    }

    /**
     * retrieves oro optionally creates a specific child containment rule corresponding to the class with a name passed in
     * @param aInClassGName global name of the associated contained class
     * @return child containment rule found or created
     */
    public synchronized MChild getMChild(String aInClassGName, boolean aInCreateIfNotFound)
    {
        MChild lMChild = getMChild(aInClassGName);
        if (null == lMChild && aInCreateIfNotFound)
        {
            lMChild = getMChild(aInClassGName);
            if (null == lMChild)
            {
                lMChild = new MChild(this, aInClassGName);
            }
        }
        return lMChild;
    }

    /**
     * retrieves all child classes associated with a parent
     * @param aOut map of classes of contained/child objects
     * @param aInResolveToConcrete specifies whether classes need to be resolved to concrete.
     */
    public void getChildClasses(Map<Ident,MClass> aOut, boolean aInResolveToConcrete)
    {
        LinkedList<Item> lItems = new LinkedList<Item>();
        getChildItems(MChild.MY_CAT, lItems);
//        System.out.println(this + ".getChildClasses("+ aInResolveToConcrete + ") has child items: " + (!lItems.isEmpty()));
        for (Item lIt : lItems)
        {
            MChild lChild = (MChild) lIt;
            MClass lThat = lChild.getTarget();

//            System.out.println(this + ".getChildClasses("+ aInResolveToConcrete + ") CHILD: " + lChild + " :: " + lThat);

            if (aInResolveToConcrete && !lThat.isConcrete())
            {
//                System.out.println(this + ".getChildClasses(): resolving subclasses; concrete: " + lThat.isConcrete());
                lThat.getSubclasses(aOut,false,aInResolveToConcrete);
            }
            else
            {
//                System.out.println(this + ".getChildClasses(): no need to resolve subclasses; concrete: " + lThat.isConcrete());
                aOut.put(lThat.getGID(), lThat);
            }
        }
    }
}
