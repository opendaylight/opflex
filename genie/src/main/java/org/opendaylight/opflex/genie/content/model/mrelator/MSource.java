package org.opendaylight.opflex.genie.content.model.mrelator;

import org.opendaylight.opflex.genie.engine.model.Cardinality;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.RelatorCat;

/**
 * Created by midvorki on 8/5/14.
 */
public class MSource extends MRelatorRuleItem
{
    public static final Cat MY_CAT = Cat.getCreate("rel:source");
    public static final RelatorCat TARGET_CAT = RelatorCat.getCreate("rel:source:target", Cardinality.SINGLE);

    public MSource(
            MRelated aInTarget,
            String aInChildGname)
    {
        super(MY_CAT, aInTarget, TARGET_CAT, aInChildGname);
    }
}