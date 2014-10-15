package org.opendaylight.opflex.genie.content.model.mconst;

import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/9/14.
 *
 * Value corresponding to the containing constant
 */
public class MValue extends Item
{

    /**
     * item category for all of the constant values. this is where all values are globally registered.
     */
    public static final Cat MY_CAT = Cat.getCreate("mconst:value");

    /**
     * hardcoded local name for the indirection. there can only be one indirection for a constant
     */
    public static final String NAME = "value";

    /**
     * Constructor
     * @param aInConst containing constant to which this value corresponds
     * @param aInValue
     */
    public MValue(MConst aInConst, String aInValue)
    {
        super(MY_CAT,aInConst,NAME);
        if (!aInConst.getAction().hasValue())
        {
            Severity.DEATH.report(
                    aInConst.toString(),
                    "definition of const value",
                    "can't specify const value",
                    "can't specify const value for constant with action: " +
                    aInConst.getAction());
        }

        value = aInValue;
    }

    /**
     * value accessor
     * @return actual value content of this value specification
     */
    public String getValue()
    {
        return value;
    }

    /**
     * Containing constant accessor. Retrieves containing constant to which this value corresponds.
     * @return
     */
    public MConst getConst()
    {
        return (MConst) getParent();
    }

    /**
     * accessor of data type for the constant for which this value is defined.
     * @return datatype definition for the constants in scope
     */
    public MType getType()
    {
        return getConst().getType(false);
    }

    /**
     * accessor of base data type for the constant for which this value is defined.
     * @return base datatype definition for the constants in scope
     */
    public MType getBaseType()
    {
        return getType().getBase();
    }

    private String value;
}
