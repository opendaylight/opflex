package org.opendaylight.opflex.genie.content.parse.pdataowner;

import org.opendaylight.opflex.genie.content.model.mownership.DefinitionScope;
import org.opendaylight.opflex.genie.content.model.mownership.MOwner;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.model.ProcessorNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 9/27/14.
 */
public class POwnerNode extends ParseNode
{
    public POwnerNode(String aInName)
    {
        super(aInName);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        return new Pair<ParseDirective, Item>(
                ParseDirective.CONTINUE,
                MOwner.get(aInData.getNamedValue(Strings.NAME,"",true), true));
    }

    protected void addParent(ProcessorNode aInParent)
    {
        super.addParent(aInParent);
        scope = DefinitionScope.get(aInParent.getName());
    }

    private DefinitionScope scope = null;
}
