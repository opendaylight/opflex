package org.opendaylight.opflex.genie.content.parse.ploader;

import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;

/**
 * Created by midvorki on 7/25/14.
 */
public class PDomainNode extends ParseNode
{
    public PDomainNode(String aInName)
    {
        super(aInName);
    }

    public Pair<ParseDirective, Item> beginCB(Node aInData, Item aInParentItem)
    {
        // DO NOTHING
        return null;
    }
}