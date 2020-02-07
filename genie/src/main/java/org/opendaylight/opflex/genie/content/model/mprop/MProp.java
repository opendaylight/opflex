package org.opendaylight.opflex.genie.content.model.mprop;

import java.util.Collection;
import java.util.LinkedList;
import java.util.Map;
import java.util.TreeMap;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.mclass.SubStructItem;
import org.opendaylight.opflex.genie.content.model.mconst.MConst;
import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.engine.model.*;
import org.opendaylight.opflex.modlan.report.Severity;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by dvorkinista on 7/7/14.
 */
public class MProp extends SubStructItem
{
    public static final Cat MY_CAT = Cat.getCreate("mprop");
    public static final RelatorCat TYPE_CAT = RelatorCat.getCreate("prop:type", Cardinality.SINGLE);

    public MProp(
            MClass aInParent,
            String aInLName,
            PropAction aInAction)
    {
        super(MY_CAT, aInParent, aInLName);
        action = aInAction;
        if (action.isDefine())
        {
            // IF THIS IS ORIGINAL DEFINITION, JUST SET THE PROP GROUP AS DEFAULT. WON'T HURT
            setGroup(Strings.DEFAULT);
        }
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // PROPERTY ACTION APIs
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Determines if this property is the original base definition.
     * Synonymous with isBase().
     * @return true if this property is the original base definition
     */
    public boolean isDefine()
    {
        return action.isDefine();
    }

    /**
     * Determines if this property is the original base definition.
     * Synonymous with isDefine().
     * @return true if this property is the original base definition
     */
    public boolean isBase()
    {
        return isDefine();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // TYPE APIs
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Associates type with the property. Only works with base property definitions (BY DESIGN - MD)
     * @param aInTypeGName global name of the type
     */
    public void addType(String aInTypeGName)
    {
        if (isBase())
        {
            TYPE_CAT.add(MY_CAT, getGID().getName(), MType.MY_CAT, aInTypeGName);
        }
        else
        {
            Severity.DEATH.report(
                    this.toString(),
                    "property type registration",
                    "can't override type",
                    "can't specify type in property override:  " + aInTypeGName);
        }
    }

    /**
     * retrieves relator representing type relationship
     * @return relator that represents type relationship, null if doesn't exist
     */
    public Relator getTypeRelator()
    {
        return TYPE_CAT.getRelator(getGID().getName());
    }

    /**
     * Retrieves this property's data type.
     * @param aInIsBaseType specifies whether to retrieve the base type or the immediate type that the base property uses
     * @return property data type
     */
    public MType getType(boolean aInIsBaseType)
    {
        MProp lBaseProp = getBase();
        Relator lRel = lBaseProp.getTypeRelator();
        MType lType = (MType) (null == lRel ? null : lRel.getToItem());
        if (null == lType)
        {
            Severity.DEATH.report(
                    this.toString(),
                    "property type retrieval",
                    "can't retrieve property type",
                    "base property definition " + lBaseProp +
                    " does not have type defined: all base property definitions required to have a type:" +
                    " no defaults assumed");
        }
        return aInIsBaseType ? lType.getBase() : lType;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // PROPERTY INHERITANCE/OVERRIDE APIs
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * retrieves base property definition
     * @return base property for this property, if this is a base/definition property
     */
    public MProp getBase()
    {
        return isBase() ? this : getOverridden(true);
    }

    /**
     * retrieves the property that this property overrides
     * @param aInIsBase specifies if only the base definition is to be found. If false, the closest def is found
     * @return property that this property overrides
     */
    public MProp getOverridden(boolean aInIsBase)
    {
        //Severity.WARN.report(this.toString(),"getOverridden()","",":::isBase:" + isBase());
        MProp lRet = null;
        if (!isBase())
        {

            for (MClass lThisClass = getMClass().getSuperclass();
                 null != lThisClass;
                 lThisClass = lThisClass.getSuperclass())
            {
                lRet = lThisClass.getProp(getLID().getName());
                if (null != lRet && (lRet.isBase() || (!aInIsBase))) // if we need base and this is base OR we don't care about base
                {
                    //Severity.WARN.report(this.toString(),"getOverridden()","","++++:::ret:::" + lRet);

                    return lRet;
                }
            }
            Severity.DEATH.report(
                    this.toString(),
                    "override target retrieval",
                    "no target found",
                    "no base definition is found for prop " + getLID().getName());
        }
        //Severity.WARN.report(this.toString(),"getOverridden()","",":::ret:" + lRet);
        return lRet;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CONSTANT APIs
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * retrieves all constants defined under this property
     * @param aOut  All constants defined under this property
     */
    public void getConst(Map<String, MConst> aOut)
    {
        Collection<Item> lItems = new LinkedList<>();
        getChildItems(MConst.MY_CAT,lItems);
        for (Item lItem : lItems)
        {
            if (!aOut.containsKey(lItem.getLID().getName()))
            {
                aOut.put(lItem.getLID().getName(), (MConst) lItem);
            }
        }
    }

    public Collection<MConst> getConst()
    {
        Map<String,MConst> lConsts = new TreeMap<>();
        getConst(lConsts);
        return lConsts.values();
    }

    /**
     * retrieves all constants defined under this property or, if specified, any of the target overridden properties
     * @param aOut  All constants defined under this property or, if specified, any of the target overridden properties
     * @param aInCheckSuperProps identifies that overridden properties are to be checked
     */
    public void findConst(Map<String, MConst> aOut, boolean aInCheckSuperProps)
    {
        for (MProp lThisProp = this;
             null != lThisProp;
             lThisProp = aInCheckSuperProps ? lThisProp.getOverridden(false) : null)
        {
            getConst(aOut);
            if (lThisProp.isBase())
            {
                lThisProp.getType(false).findConst(aOut,true);
            }
        }
    }

    public Item getNextConstantHolder()
    {
        for (MProp lThisProp = this;
             null != lThisProp;
             lThisProp = lThisProp.getOverridden(false))
        {
            if (this != lThisProp && lThisProp.hasConstants())
            {
                return lThisProp;
            }
            else if (lThisProp.isBase())
            {
                return lThisProp.getType(false).getClosestConstantHolder();
            }
        }
        return null;
    }

    public boolean hasConstants()
    {
        return hasChildren(MConst.MY_CAT);
    }

    public boolean hasEnumeratedConstants()
    {
        return getType(true).getTypeHint().getInfo().isEnumerated() && hasConstants();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // PROP GROUP APIs
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * property group name accessor: retrieves properrty group name for this property
     * @return property group name
     */
    public String getGroup()
    {
        return Strings.isEmpty(group) ?
                (isBase() ?
                    Strings.DEFAULT :
                    getOverridden(false).getGroup()) :
                group;
    }

    /**
     * property group name mutator: sets property group name
     * @param aIn property group name
     */
    public void setGroup(String aIn)
    {
        group = aIn;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // VALIDATOR APIs
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    public void setLocalIdx(int aIn)
    {
        localIdx = aIn;
    }

    public int getLocalIdx()
    {
        return getBase().localIdx;
    }

    public int getPropId(MClass aInConcreteClass)
    {
        return (aInConcreteClass.getGID().getId() << 15) | getLocalIdx();
    }


    private String group = null;
    private final PropAction action;
    private int localIdx = -1;
}
