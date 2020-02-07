package org.opendaylight.opflex.genie.content.model.mvalidation;

import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/10/14.
 */
public class MConstraint extends Item
{
    protected MConstraint(Cat aInCat, MValidator aInValidator, String aInName)
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
    }
}
