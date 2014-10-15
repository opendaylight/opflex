package org.opendaylight.opflex.genie.content.model.mcont;

import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.RelatorCat;

/**
 * Created by dvorkinista on 7/8/14.
 *
 * Abstraction of the containment rule item, a superclass of rules specifying the relationship
 * with containment dependencies.
 */

public abstract class MContRuleItem
        extends MContItem
{
    /**
     * Constructor
     * @param aInMyCat category of a containment item
     * @param aInParent parent item under which this item is created
     * @param aInTargetRelatorCat target class resolution category
     * @param aInTargetGname global name of the target class
     */
    protected MContRuleItem(
            Cat aInMyCat, MContItem aInParent, RelatorCat aInTargetRelatorCat, String aInTargetGname)
    {
        super(aInMyCat, aInParent, aInTargetRelatorCat, aInTargetGname);
    }
}