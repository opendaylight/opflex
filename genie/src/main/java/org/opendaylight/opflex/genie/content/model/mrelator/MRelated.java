package org.opendaylight.opflex.genie.content.model.mrelator;

import java.util.Collection;
import java.util.LinkedList;
import java.util.Map;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.engine.model.*;

/**
 * Created by midvorki on 8/5/14.
 */
public class MRelated
        extends MRelatorItem
{
    /**
     * Related category definition. Contains registry of all related definitions.
     */
    public static final Cat MY_CAT = Cat.getCreate("rel:related");

    /**
     * Related target class category definition. Contains references between relateds and their represented classes.
     */
    public static final RelatorCat TARGET_CAT = RelatorCat.getCreate("rel:related:targetref", Cardinality.SINGLE);

    /**
     * rule registration API. Adds or finds containment rule. This API is scoped to the package and is for internal use.
     * @param aInTargetGName name of the target class
     * @param aInSourceGName name of the source class
     * @return related corresponding to the target name passed in.
     */
    static final synchronized  MRelated addRule(String aInTargetGName, String aInSourceGName)
    {
        MRelated lContr = MRelated.get(aInTargetGName, true);
        lContr.getMSource(aInSourceGName, true);

        return lContr;
    }

    /**
     * Constructor.
     * @param aInTargetGname name of the the class for whom the related rule is allocated
     */
    MRelated(String aInTargetGname)
    {
        super(MY_CAT, null, TARGET_CAT, aInTargetGname);
    }

    /**
     * Related instance finder
     * @param aInGName name of the class for which "related" is to be found
     * @return related associated with the class corresponding to the name passed in.
     */
    public static MRelated get(String aInGName)
    {
        return (MRelated) MY_CAT.getItem(aInGName);
    }

    /**
     * retrieves or creates a related. This is an internal API used within te package.
     * @param aInGName name of the class for which related is resolved.
     * @param aInCreateIfNotFound specifies whether related needs to be created if not found
     * @return related associated with a class corresponding to the name passed in
     */
    static synchronized MRelated get(String aInGName, boolean aInCreateIfNotFound)
    {
        MRelated lContr = get(aInGName);
        if (null == lContr && aInCreateIfNotFound)
        {
            lContr = new MRelated(aInGName);
        }
        return lContr;
    }

    /**
     * checks if there are per source rules per this related
     * @return true if this related has sourceren containment rules.
     */
    public boolean hasSource()
    {
        return hasChildren(MSource.MY_CAT);
    }

    /**
     * retrieves source containment rules specified within this related.
     * @param aOut set of source containement rules
     */
    public void getSourceren(Collection<MSource> aOut)
    {
        LinkedList<Item> lItems = new LinkedList<Item>();
        getChildItems(MSource.MY_CAT, lItems);
        for (Item lIt : lItems)
        {
            aOut.add((MSource)lIt);
        }
    }

    /**
     * retrieves specific source containment rule corresponding to the class with a name passed in
     * @param aInClassGName global name of the associated relator class
     * @return source containment rule
     */
    public MSource getMSource(String aInClassGName)
    {
        return (MSource) getChildItem(MSource.MY_CAT,aInClassGName);
    }

    /**
     * retrieves oro optionally creates a specific source containment rule corresponding to the class with a name passed in
     * @param aInClassGName global name of the associated relator class
     * @return source containment rule found or created
     */
    public synchronized MSource getMSource(String aInClassGName, boolean aInCreateIfNotFound)
    {
        MSource lMSource = getMSource(aInClassGName);
        if (null == lMSource && aInCreateIfNotFound)
        {
            lMSource = getMSource(aInClassGName);
            if (null == lMSource)
            {
                lMSource = new MSource(this, aInClassGName);
            }
        }
        return lMSource;
    }

    /**
     * retrieves all source classes associated with a target
     * @param aOut map of classes of relator/source objects
     * @param aInResolveToConcrete specifies whether classes need to be resolved to concrete.
     */
    public void getSourceClasses(Map<Ident,MClass> aOut, boolean aInResolveToConcrete)
    {
        LinkedList<Item> lItems = new LinkedList<Item>();
        getChildItems(MSource.MY_CAT, lItems);
//        System.out.println(this + ".getSourceClasses("+ aInResolveToConcrete + ") has source items: " + (!lItems.isEmpty()));
        for (Item lIt : lItems)
        {
            MSource lSource = (MSource) lIt;
            MClass lThat = lSource.getTarget();

//            System.out.println(this + ".getSourceClasses("+ aInResolveToConcrete + ") SOURCE: " + lSource + " :: " + lThat);

            if (aInResolveToConcrete && !lThat.isConcrete())
            {
//                System.out.println(this + ".getSourceClasses(): resolving subclasses; concrete: " + lThat.isConcrete());
                lThat.getSubclasses(aOut,false,aInResolveToConcrete);
            }
            else
            {
//                System.out.println(this + ".getSourceClasses(): no need to resolve subclasses; concrete: " + lThat.isConcrete());
                aOut.put(lThat.getGID(), lThat);
            }
        }
    }
}
