package org.opendaylight.opflex.genie.content.model.mownership;

import java.util.LinkedList;
import java.util.Map;
import java.util.TreeMap;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;

/**
 * Created by midvorki on 9/27/14.
 * Specification of the owner in the context of data ownership rules. Any object, property etc has an owner.
 * Owner is the unique mutative accessor of information (accessor, a process, thread etc. that is allowed to write)
 * who is responsible for the lifecylce management of corresponding data
 */
public class MOwner extends MOwnershipComponent
{
    /**
     * category identifying all owner definitions
     */
    public static final Cat MY_CAT = Cat.getCreate("mowner");

    public static synchronized MOwner get(String aInName, boolean aInCreateIfNotFound)
    {
        MOwner lOwner = (MOwner) MY_CAT.getItem(aInName);
        if (null == lOwner && aInCreateIfNotFound)
        {
            lOwner = new MOwner(aInName);
        }
        return lOwner;
    }

    /**
     * Constructor.
     * @param aInName name of the owner.
     */
    private MOwner(String aInName)
    {
        super(MY_CAT,null,aInName, DefinitionScope.GLOBAL);
    }

    public void getClasses(Map<String, MClass> aOut)
    {
        // GO THROUGH IMMEDIATE CLASS RULES
        {
            LinkedList<Item> lIts = new LinkedList<Item>();
            getChildItems(MClassRule.MY_CAT, lIts);
            for (Item lIt : lIts)
            {
                ((MClassRule)lIt).getClasses(aOut);
            }
        }
        // GO THROUGH MODULE RULES
        {
            LinkedList<Item> lIts = new LinkedList<Item>();
            getChildItems(MModuleRule.MY_CAT, lIts);
            for (Item lIt : lIts)
            {
                ((MModuleRule)lIt).getClasses(aOut);
            }
        }
    }

    public void postLoadCb()
    {
        super.postLoadCb();
        TreeMap<String,MClass> lClasses = new TreeMap<String, MClass>();
        getClasses(lClasses);
        for (MClass lClass : lClasses.values())
        {
            lClass.addOwner(getLID().getName());
        }
    }
}
