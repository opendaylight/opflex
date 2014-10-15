package org.opendaylight.opflex.genie.content.parse.pnaming;

import org.opendaylight.opflex.genie.content.model.mnaming.MNamer;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 8/1/14.
 */
public class PParentNode extends ParseNode
{
    public PParentNode(String aInName)
    {
        super(aInName);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        return new Pair<ParseDirective, Item>(
                            ParseDirective.CONTINUE,
                            ((MNamer)aInParentItem).getNameRule(
                                aInData.getNamedValue(
                                     Strings.CLASS,
                                     aInData.getNamedValue(Strings.NAME, null, false),
                                     true),
                                true));
    }
}
