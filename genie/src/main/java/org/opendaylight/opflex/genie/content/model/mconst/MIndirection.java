package org.opendaylight.opflex.genie.content.model.mconst;

import org.opendaylight.opflex.genie.content.model.mprop.MProp;
import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by dvorkinista on 7/9/14.
 *
 * Specification of constant indirection directives. Identifies the constant referenced by the parent
 * constant of this object. This is used for specifying default values, transients, as well as aliases
 */
public class MIndirection extends Item
{
    /**
     * item category for all of the constant indirections. this is where all indirections are globally registered.
     */
    public static final Cat MY_CAT = Cat.getCreate("mconst:indirection");

    /**
     * hardcoded local name for the indirection. there can only be one indirection for a constant
     */
    public static final String NAME = "indirection";

    /**
     * Constructor
     * @param aInConst constant that has the indirection specified
     * @param aInConstName name of the target constant referenced
     */
    public MIndirection(MConst aInConst, String aInConstName)
    {
        super(MY_CAT,aInConst,NAME);
        target = aInConstName;
        if (!aInConst.getAction().hasExplicitIndirection())
        {
            Severity.DEATH.report(
                    aInConst.toString(),
                    "definition of target const",
                    "can't specify target indirection",
                    "can't specify target const " + target + " for constant with action: " +
                    aInConst.getAction());
        }
    }

    /**
     * accessor of the target constant name
     * @return
     */
    public String getTargetName()
    {
        return target;
    }

    /**
     * accessor of the target constant
     * @return constant that this indirection references
     */
    public MConst findTargetConst()
    {
        MConst lConst = null;
        Item lItem = getParent().getParent();
        if (lItem instanceof MType)
        {
            lConst = ((MType)lItem).findConst(getTargetName(), true);
        }
        else
        {
            lConst = ((MProp)lItem).findConst(getTargetName(), true);
        }
        if (null == lConst)
        {
            Severity.DEATH.report(
                    this.toString(),
                    "retrieval of target const",
                    "no constant found",
                    "can't find target const " + target + " in context of " +
                    lItem);
        }
        return lConst;
    }

    /**
     * parent constant accessor. retrieves containing constant for which indirection is specified.
     * @return constant for which this indirection is specified.
     */
    public MConst getConst()
    {
        return (MConst) getParent();
    }

    /**
     * accessor of data type for the constant for which this indirection is defined. also the datatype definition of the
     * constant referenced.
     * @return datatype definition for the constants in scope
     */
    public MType getType()
    {
        return getConst().getType(false);
    }

    /**
     * accessor of base data type for the constant for which this indirection is defined. also the base datatype
     * definition of the constant referenced.
     * @return base datatype definition for the constants in scope
     */
    public MType getBaseType()
    {
        return getType().getBase();
    }

    /**
     * name of the referenced/target constant
     */
    private String target;

}
