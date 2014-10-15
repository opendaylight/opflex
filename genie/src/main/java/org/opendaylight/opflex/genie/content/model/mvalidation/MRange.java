package org.opendaylight.opflex.genie.content.model.mvalidation;

import java.util.Collection;
import java.util.LinkedList;
import java.util.Map;

import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;

/**
 * Created by midvorki on 7/10/14.
 */
public class MRange
        extends MConstraint
{
    public static final Cat MY_CAT = Cat.getCreate("mvalidator:mrange");

    public MRange(MValidator aInParent, String aInName, ValidatorAction aInActionOrNull)
    {
        super(MY_CAT, aInParent, aInName, aInActionOrNull);
    }

    public MType getType(boolean aInIsBaseType)
    {
        return getValidator().getType(aInIsBaseType);
    }

    public MRange getSuper()
    {
        return (MRange) getSuperConstraint();
    }

    public MConstraintValue getConstraintValue(ConstraintValueType aIn)
    {
        return (MConstraintValue) getChildItem(MConstraintValue.MY_CAT, aIn.getName());
    }

    public void getConstraintValue(Map<ConstraintValueType, MConstraintValue> aOut)
    {
        Collection<Item> lItems = new LinkedList<Item>();
        getChildItems(MConstraintValue.MY_CAT, lItems);
        for (Item lIt : lItems)
        {
            MConstraintValue lCV = (MConstraintValue) lIt;
            if (!aOut.containsKey(lCV.getConstraintValueType()))
            {
                aOut.put(lCV.getConstraintValueType(), lCV);
            }
        }
    }

    public MConstraintValue findConstraintValue(ConstraintValueType aIn, boolean aInIncludeSuper)
    {
        MConstraintValue lValue = null;
        for (MRange lRange = this;
             null != lRange && null == lValue;
             lRange = aInIncludeSuper ? lRange.getSuper() : null)
        {
            lValue = lRange.getConstraintValue(aIn);
        }
        return lValue;
    }

    public void findConstraintValue(Map<ConstraintValueType, MConstraintValue> aOut, boolean aInIncludeSuper)
    {
        MConstraintValue lValue = null;
        for (MRange lRange = this;
             null != lRange;
             lRange = aInIncludeSuper ? lRange.getSuper() : null)
        {
            getConstraintValue(aOut);
        }
    }
}
