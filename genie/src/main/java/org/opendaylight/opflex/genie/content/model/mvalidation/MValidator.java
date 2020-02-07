package org.opendaylight.opflex.genie.content.model.mvalidation;

import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;

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

    public MValidator(Item aInParent, String aInName, ValidatorAction aInAction)
    {
        super(MY_CAT, aInParent, aInName);
        action = aInAction;
    }

    public ValidatorAction getAction()
    {
        return action;
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // TYPE API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // SUPER-VALIDATOR API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    private final ValidatorAction action;
}
