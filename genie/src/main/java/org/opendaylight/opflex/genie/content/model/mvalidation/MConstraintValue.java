package org.opendaylight.opflex.genie.content.model.mvalidation;

import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;

/**
 * Created by dvorkinista on 7/10/14.
 */
public class MConstraintValue
        extends Item
{
    public static final Cat MY_CAT = Cat.getCreate("mconst:value");
    public static final String NAME = "value";

    public MConstraintValue(MRange aInVal, String aInValue, ConstraintValueType aInConstraintValueType)
    {
        super(MY_CAT,aInVal,aInConstraintValueType.getName());
        value = aInValue;
        constraintValueType = aInConstraintValueType;
    }

    public String getValue()
    {
        return value;
    }

    public MRange getRange()
    {
        return (MRange) getParent();
    }

    public MValidator getValidator()
    {
        return getRange().getValidator();
    }

    public MType getType(boolean aInIsBase)
    {
        return getValidator().getType(aInIsBase);
    }

    public ConstraintValueType getConstraintValueType()
    {
        return constraintValueType;
    }

    private final String value;
    private final ConstraintValueType constraintValueType;
}