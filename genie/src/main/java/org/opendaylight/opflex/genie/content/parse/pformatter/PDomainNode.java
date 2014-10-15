package org.opendaylight.opflex.genie.content.parse.pformatter;

import org.opendaylight.opflex.genie.content.model.mformatter.MFormatterDomain;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/24/14.
 */
public class PDomainNode extends ParseNode
{
    public PDomainNode(String aInName)
    {
        super(aInName);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        MFormatterDomain lDom = new MFormatterDomain(aInData.getNamedValue(Strings.NAME,null, true));
        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE,lDom);
    }
}
