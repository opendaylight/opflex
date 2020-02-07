package org.opendaylight.opflex.genie.content.model.mconst;

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
        if (!aInConst.getAction().hasExplicitIndirection())
        {
            Severity.DEATH.report(
                    aInConst.toString(),
                    "definition of target const",
                    "can't specify target indirection",
                    "can't specify target const " + aInConstName + " for constant with action: " +
                    aInConst.getAction());
        }
    }
}
