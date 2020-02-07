package org.opendaylight.opflex.genie.content.model.mconst;

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

    private final String value;
}
