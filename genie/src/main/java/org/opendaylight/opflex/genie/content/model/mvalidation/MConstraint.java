package org.opendaylight.opflex.genie.content.model.mvalidation;

import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/10/14.
 */
public class MConstraint extends Item
{
    protected MConstraint(Cat aInCat, MValidator aInValidator, String aInName, ValidatorAction aInAction)
    {
        super(aInCat, aInValidator, aInName);
        if (ValidatorAction.REMOVE == aInValidator.getAction())
        {
            Severity.DEATH.report(
                    this.toString(),
                    "constraint definition",
                    "constraint can't be defined",
                    "constraint can only be defined under non-removed validators: " + aInValidator.getAction());
        }
        action = (null == aInAction) ?
                    (ValidatorAction.ADD == aInValidator.getAction() ?
                                            ValidatorAction.CLOBBER : aInValidator.getAction()) :
                    (ValidatorAction.ADD == aInAction ?
                            ValidatorAction.CLOBBER : aInAction);

    }

    public ValidatorAction getAction()
    {
        return action;
    }

    public MValidator getValidator()
    {
        return (MValidator) getAncestorOfCat(MValidator.MY_CAT);
    }

    public MConstraint getSuperConstraint()
    {
        MConstraint lConstraint = null;
        if (ValidatorAction.REMOVE != getAction())
        {
            for (MValidator lValidator = getValidator().getSuperValidator();
                 null != lValidator && null == lConstraint; lValidator = lValidator.getSuperValidator())
            {
                lConstraint = (MConstraint) lValidator.getChildItem(getCat(), getLID().getName());
            }
        }
        return lConstraint;
    }

    private final ValidatorAction action;
}
