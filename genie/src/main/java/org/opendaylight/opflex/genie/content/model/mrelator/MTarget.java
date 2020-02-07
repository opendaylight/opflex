package org.opendaylight.opflex.genie.content.model.mrelator;

import org.opendaylight.opflex.genie.engine.model.Cardinality;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.RelatorCat;

/**
 * Created by midvorki on 8/5/14.
 */
public class MTarget extends MRelatorRuleItem
{
    public static final Cat MY_CAT = Cat.getCreate("rel:target");
    public static final RelatorCat TARGET_CAT = RelatorCat.getCreate("rel:target:targetref", Cardinality.SINGLE);

    MTarget(
            MRelator aInSource,
            String aInTargetGname)
    {
        super(MY_CAT, aInSource, TARGET_CAT, aInTargetGname);
    }

    public MRelator getRelator()
    {
        return (MRelator) getParent();
    }
}
