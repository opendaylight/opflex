package org.opendaylight.opflex.genie.content.parse.pvalidation;

import org.opendaylight.opflex.genie.content.model.mvalidation.*;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/22/14.
 */
public class PRangeNode
        extends ParseNode
{
    public PRangeNode(String aInName)
    {
        super(aInName);
        ValidatorAction action = ValidatorAction.get(aInName);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        MRange lRange = new MRange(
                (MValidator)aInParentItem,
                aInData.getNamedValue(Strings.NAME,Strings.DEFAULT,true));

        return new Pair<>(ParseDirective.CONTINUE,lRange);
    }

}
