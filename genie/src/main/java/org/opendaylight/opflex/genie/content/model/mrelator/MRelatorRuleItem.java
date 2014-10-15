package org.opendaylight.opflex.genie.content.model.mrelator;

import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.RelatorCat;

/**
 * Created by midvorki on 8/5/14.
 */
public class MRelatorRuleItem
        extends MRelatorItem
{
    /**
     * Constructor
     * @param aInMyCat category of a containment item
     * @param aInParent parent item under which this item is created
     * @param aInTargetRelatorCat target class resolution category
     * @param aInTargetGname global name of the target class
     */
    protected MRelatorRuleItem(
            Cat aInMyCat, MRelatorItem aInParent, RelatorCat aInTargetRelatorCat, String aInTargetGname)
    {
        super(aInMyCat, aInParent, aInTargetRelatorCat, aInTargetGname);
    }
}