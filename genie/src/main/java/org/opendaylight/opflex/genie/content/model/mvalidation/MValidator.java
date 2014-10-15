package org.opendaylight.opflex.genie.content.model.mvalidation;

import java.util.Map;

import org.opendaylight.opflex.genie.content.model.mprop.MProp;
import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/10/14.
 *
 * Validator is a collection of constraints imposed onto a property or a type.
 * Validators are inheritable, that property inherits validator from other properties
 * it overrides or from the type it consumes. Validators have actions: CLOBBER, ADD, REMOVE. ADD
 * specifies that validator is added (default case) or overriding. In this case, if there's at least one
 * inherited constraint defined in the path, all of the items will be merged. CLOBBER is like add, but implies
 * complete override. REMOVE implies removal of validators with given name.
 *
 * Validator consist of multiple range and content constraints.
 * All of the constraints of given type in the validator are logically OR'ed. Constraints are AND'ed
 * across types of constraints. For example, one at least one of ranges has to be satisfied, if any exist,
 * together with at least one of content constraints, if any exists. Therefore, for validator to pass,
 * at least one of the range constraints and at least one of the content constraints are to be satisfied.
 */
public class MValidator extends Item
{
    public static final Cat MY_CAT = Cat.getCreate("mvalidator");

    public MValidator(MProp aInParent, String aInName, ValidatorAction aInAction)
    {
        this((Item)aInParent, aInName, ValidatorScope.PROPERTY, aInAction);
    }

    public MValidator(MType aInParent, String aInName, ValidatorAction aInAction)
    {
        this((Item)aInParent, aInName, ValidatorScope.TYPE, aInAction);
    }

    public MValidator(Item aInParent, String aInName, ValidatorScope aInScope, ValidatorAction aInAction)
    {
        super(MY_CAT, aInParent, aInName);
        scope = aInScope;
        action = aInAction;
    }

    public ValidatorScope getScope()
    {
        return scope;
    }

    public ValidatorAction getAction()
    {
        return action;
    }

    public MProp getProp()
    {
        return ValidatorScope.PROPERTY == scope ? (MProp) getParent() : null;
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // TYPE API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    public MType getType(boolean aInIsBaseType)
    {
        Item lParent = getParent();
        MType lType =  ValidatorScope.TYPE == scope ? (MType) lParent : ((MProp) lParent).getType(false);
        if (null == lType)
        {
            Severity.DEATH.report(
                    this.toString(),
                    "const type retrieval",
                    "type definition not found",
                    "no type is resolvable for validator in context of " +
                    lParent);
        }
        return aInIsBaseType ? lType.getBase() : lType;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // PROP API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    public MProp getProp(boolean aInBaseProp)
    {
        Item lParent = getParent();

        if (ValidatorScope.PROPERTY == scope)
        {
            return aInBaseProp ? (((MProp)lParent).getBase()) : (MProp) lParent;
        }
        else
        {
            Severity.DEATH.report(
                    this.toString(),
                    "const property retrieval",
                    "no associated property found",
                    "validator is contained by " +
                    lParent);

            return null;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // SUPER-VALIDATOR API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    public MValidator getSuperValidator()
    {
        Item lParent = getParent();
        if (ValidatorAction.REMOVE != getAction())
        {
            if (ValidatorScope.TYPE == scope)
            {
                MType lType = (MType) lParent;
                if (!lType.isBase())
                {
                    lType = lType.getSupertype();
                    if (null != lType)
                    {
                        lType.findValidator(getLID().getName(), true);
                    }
                    else
                    {
                        Severity.DEATH.report(this.toString(), "retrieval of super validator", "no super validator",
                                              "super validator can't be found for non-base type " + lParent);
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
                        lProp.findValidator(getLID().getName(), true);
                    }
                    else
                    {
                        Severity.DEATH.report(this.toString(), "retrieval of super validator", "no super prop",
                                              "super validator can't be found for non-base prop " + lParent);
                    }
                }
            }
        }
        return null;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CONTENT VALIDATOR API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    public MContentValidator getContentValidator(String aInName)
    {
        return (MContentValidator) getChildItem(MContentValidator.MY_CAT,aInName);
    }

    public MContentValidator findContentValidator(String aInName, boolean aInCheckSuper)
    {
        MContentValidator lCV = null;
        for (MValidator lV = this;
             null != lV && null == lCV;
             lV = aInCheckSuper ? lV.getSuperValidator() : null)
        {
            lCV = lV.getContentValidator(aInName);
        }
        return lCV;
    }

    public void findContentValidator(Map<String,MContentValidator> aOut, boolean aInCheckSuper)
    {
        MContentValidator lCV = null;
        for (MValidator lV = this;
             null != lV && null == lCV;
             lV = aInCheckSuper ? lV.getSuperValidator() : null)
        {
            if (!aOut.containsKey(lCV.getLID().getName()))
            {
                aOut.put(lCV.getLID().getName(), lCV);
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CONTENT VALIDATOR API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    public MRange getRange(String aInName)
    {
        return (MRange) getChildItem(MRange.MY_CAT,aInName);
    }

    public MRange findRange(String aInName, boolean aInCheckSuper)
    {
        MRange lRB = null;
        for (MValidator lV = this;
             null != lV && null == lRB;
             lV = aInCheckSuper ? lV.getSuperValidator() : null)
        {
            lRB = lV.getRange(aInName);
        }
        return lRB;
    }

    public void findRange(Map<String,MRange> aOut, boolean aInCheckSuper)
    {
        MRange lRB = null;
        for (MValidator lV = this;
             null != lV && null == lRB;
             lV = aInCheckSuper ? lV.getSuperValidator() : null)
        {
            if (!aOut.containsKey(lRB.getLID().getName()))
            {
                aOut.put(lRB.getLID().getName(), lRB);
            }
        }
    }

    private final ValidatorScope scope;
    private final ValidatorAction action;
}
