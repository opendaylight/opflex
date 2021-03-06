package org.opendaylight.opflex.genie.content.model.mconst;

import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;

/**
 * Created by dvorkinista on 7/9/14.
 */
public class MConst extends Item
{
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CATEGORIES
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * item category for all of the constants. this is where all constants are globally registered.
     */
    public static final Cat MY_CAT = Cat.getCreate("mconst");

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CONSTRUCTION
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * INTERNAL generic constructor used by other MConst constructors
     * @param aInParent parent item that is either a type or a property
     * @param aInName name of the constant
     * @param aInAction constant action that indicates this constant's behavior
     */
    public MConst(
        Item aInParent,
        String aInName,
        ConstAction aInAction)
    {
        super(MY_CAT, aInParent, aInName);
        action = (null == aInAction) ? ConstAction.VALUE : aInAction;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CONSTANT SCOPE AND ACTION API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * constant action accessor. retrieves this constant's behavioral directives
     * @return
     */
    public ConstAction getAction()
    {
        return action;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // TYPE API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // PROP API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // SUPER-CONST API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // VALUE API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * checks if this constant has value
     * @return
     */
    public boolean hasValue()
    {
        return action.hasValue() && null != getChildItem(MValue.MY_CAT,MValue.NAME);
    }

    /**
     * value accessor. retrieves value specified DIRECTLY under this constant. it DOES NOT perform any checks with any
     * of the super-constants that this constant overrodes.
     * @return
     */
    public MValue getValue()
    {
        return action.hasValue() ?
                (MValue) getChildItem(MValue.MY_CAT,MValue.NAME) :
                null;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // INDIRECTION API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    private final ConstAction action;
}
