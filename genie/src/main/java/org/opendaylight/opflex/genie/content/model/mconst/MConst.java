package org.opendaylight.opflex.genie.content.model.mconst;

import org.opendaylight.opflex.genie.content.model.mprop.MProp;
import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.modlan.report.Severity;

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
     * Constructs a constant under a property
     * @param aInParent property that holds the constant
     * @param aInName name of the constant
     * @param aInAction constant action that indicates this constant's behavior
     */
    public MConst(
            MProp aInParent,
            String aInName,
            ConstAction aInAction)
    {
        this((Item) aInParent, aInName, aInAction, ConstScope.PROPERTY);
    }

    /**
     * Constructs a constant under a atype
     * @param aInParent type that holds the constant
     * @param aInName name of the constant
     * @param aInAction constant action that indicates this constant's behavior
     */
    public MConst(
            MType aInParent,
            String aInName,
            ConstAction aInAction)
    {
        this((Item)aInParent, aInName, aInAction, ConstScope.TYPE);
    }

    /**
     * INTERNAL generic constructor used by other MConst constructors
     * @param aInParent parent item that is either a type or a property
     * @param aInName name of the constant
     * @param aInAction constant action that indicates this constant's behavior
     */
    public MConst(
            Item aInParent,
            String aInName,
            ConstAction aInAction,
            ConstScope aInScope)
    {
        super(MY_CAT, aInParent, aInName);
        action = (null == aInAction) ? ConstAction.VALUE : aInAction;
        scope = aInScope;
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

    /**
     * scope accessor. identifies whether this is a type or property bound constant
     * @return ConstScope.PROPERTY if this const is bound to property. ConstScope.TYPE if this const is bound to type.
     */
    public ConstScope getScope()
    {
        return scope;
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // TYPE API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * type accessor.
     * retrieves the type associated with this constant. type is either a direct parent of a constant or
     * associated with the property that contains this constant.
     * @param aInIsBaseType identifies whether the base/built-in type should be resolved.
     * @return type associated with this constant
     */
    public MType getType(boolean aInIsBaseType)
    {
        Item lParent = getParent();
        MType lType =  ConstScope.TYPE == scope ? (MType) lParent : ((MProp) lParent).getType(false);
        if (null == lType)
        {
            Severity.DEATH.report(
                    this.toString(),
                    "const type retrieval",
                    "type definition not found",
                    "no type is resolvable for constant in context of " +
                    lParent);
        }
        return aInIsBaseType ? lType.getBase() : lType;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // PROP API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * property accessor.
     * retrieves the property that holds this constant. if the constant is not held by property, exception is thrown.
     * @param aInBaseProp indicates whether property needs to be resolved to the base property that has type binding.
     * @return
     */
    public MProp getProp(boolean aInBaseProp)
    {
        Item lParent = getParent();

        if (ConstScope.PROPERTY == scope)
        {
            return aInBaseProp ? (((MProp)lParent).getBase()) : (MProp) lParent;
        }
        else
        {
            Severity.DEATH.report(
                    this.toString(),
                    "const property retrievale",
                    "no associated property found",
                    "const is contained by " +
                    lParent);

            return null;
        }
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // SUPER-CONST API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * super-const accessor. resolves the constant that this constant overrides by finding a property that is defined
     * in the super-type or super-property that has the same name as this constant.
     * @return constant with the same name that this constant overrides
     */
    public MConst getSuperConst()
    {
        if (ConstAction.REMOVE != getAction())
        {
            Item lParent = getParent();
            if (ConstScope.TYPE == scope)
            {
                MType lType = (MType) lParent;
                if (!lType.isBase())
                {
                    lType = lType.getSupertype();
                    if (null != lType)
                    {
                        lType.findConst(getLID().getName(), true);
                    }
                    else
                    {
                        Severity.DEATH.report(this.toString(), "retrieval of super const", "no supertype",
                                              "super type can't be found for non-base type " + lParent);
                    }
                }
            }
            else
            {
                MProp lProp = (MProp) lParent;
                if (!lProp.isBase())
                {
                    lProp = lProp.getOverridden(false);
                    if (null != lProp)
                    {
                        lProp.findConst(getLID().getName(), true);
                    }
                    else
                    {
                        Severity.DEATH.report(this.toString(), "retrieval of super const", "no super prop",
                                              "super Prop can't be found for non-base prop " + lParent);
                    }
                }
            }
        }
        return null;
    }

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

    /**
     * value accessor/finder. retrieves value specified for this constant.
     * this method inspects super-constants and indirections for the value corresponding to this constant.
     * @return value corresponding to this constant
     */
    public MValue findValue()
    {
        MValue lValue = null;
        if (action.hasDirectOrIndirectValue())
        {
            if (action.hasExplicitIndirection())
            {
                lValue = findIndirectionConst().findValue();
                if (null == lValue)
                {
                    Severity.DEATH.report(
                            this.toString(),
                            "retrieval of const value",
                            "no value found",
                            "no value found for " + scope +
                            " const with action: " + getAction().getName());
                }
            }
            else
            {
                for (MConst lConst = this; null != lConst && null == lValue; lConst = lConst.getSuperConst())
                {
                    lValue = lConst.getValue();
                }
                if (null == lValue)
                {
                    Severity.DEATH.report(
                            this.toString(),
                            "retrieval of const value",
                            "no value found",
                            "no value found for " + scope +
                            " const with action: " + getAction().getName());
                }
            }
        }
        return lValue;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // INDIRECTION API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * checks if this constant has explicitly defined indirection
     * @return
     */
    public boolean hasExplicitIndirection()
    {
        return action.hasExplicitIndirection() && null != getChildItem(MValue.MY_CAT,MValue.NAME);
    }

    /**
     * constant indirection accessor. retrieves indirection directive specified DIRECTLY under this constant.
     * it DOES NOT perform any checks with any of the super-constants that this constant overrodes.
     * @return indirection directive found or null.
     */
    public MIndirection getIndirection()
    {
        return (action.hasExplicitIndirection()) ?
                (MIndirection) getChildItem(MIndirection.MY_CAT,MIndirection.NAME) :
                null;

    }

    /**
     * constant indirection directive accessor/finder. retrieves indirection specified for this property.
     * this method inspects super-constants the target indirection corresponding to this constant.
     * @return indirection directive corresponding to this constant
     */
    public MIndirection findIndirection()
    {
        MIndirection lIndir = null;
        if (action.hasExplicitIndirection())
        {
            for (MConst lConst = this;
                 null != lConst && null == lIndir;
                 lConst = lConst.getSuperConst())
            {
                lIndir = lConst.getIndirection();
            }
            if (null == lIndir)
            {
                Severity.DEATH.report(
                        this.toString(),
                        "retrieval of const indirection",
                        "no const indirection found",
                        "const action " + getAction() +
                        " can't be satisfied");
            }
        }
        return lIndir;

    }

    /**
     * indirection target constant accessor/finder. retrieves constant associated with indirection specified
     * in this constant.
     * @return constant associated with indirection specified
     */
    public MConst findIndirectionConst()
    {
        MConst lConst = null;
        if (action.hasExplicitIndirection())
        {
            MIndirection lInd = findIndirection();
            if (null != lInd)
            {
                lConst = lInd.findTargetConst();
                if (null == lConst)
                {
                    Severity.DEATH.report(
                            this.toString(),
                            "retrieval of const value",
                            "no value found",
                            "no value found for indirection: " + lInd +
                            "for " + scope +
                            " const with action: " + getAction().getName());
                }
            }
            else
            {
                Severity.DEATH.report(
                        this.toString(),
                        "retrieval of indirection const",
                        "no const found",
                        "no indirection found for " + scope +
                        " const with action: " + getAction().getName());
            }
        }
        return lConst;
    }

    private final ConstAction action;
    private final ConstScope scope;
}
