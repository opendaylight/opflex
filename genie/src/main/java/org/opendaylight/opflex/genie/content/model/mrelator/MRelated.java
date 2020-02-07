package org.opendaylight.opflex.genie.content.model.mrelator;

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
    static synchronized  MRelated addRule(String aInTargetGName, String aInSourceGName)
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
     */
    public synchronized void getMSource(String aInClassGName, boolean aInCreateIfNotFound)
    {
        MSource lMSource = getMSource(aInClassGName);
        if (null == lMSource && aInCreateIfNotFound)
        {
            lMSource = getMSource(aInClassGName);
            if (null == lMSource)
            {
                new MSource(this, aInClassGName);
            }
        }
    }
}
