package org.opendaylight.opflex.genie.content.parse.pdataowner;

import org.opendaylight.opflex.genie.content.model.mownership.DefinitionScope;
import org.opendaylight.opflex.genie.content.model.mownership.MClassRule;
import org.opendaylight.opflex.genie.content.model.mownership.MModuleRule;
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
public class PClassNode extends ParseNode
{
    public PClassNode(String aInName)
    {
        super(aInName);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        MClassRule lRule = null;

        switch (scope)
        {
            case OWNER:

                lRule = new MClassRule((MOwner) aInParentItem, aInData.getNamedValue(Strings.NAME,Strings.ASTERISK, true));
                break;

            case MODULE:
            default:

                lRule = new MClassRule((MModuleRule) aInParentItem, aInData.getNamedValue(Strings.NAME,Strings.ASTERISK, true));
                break;
        }
        return new Pair<ParseDirective, Item>(
                        ParseDirective.CONTINUE,
                        lRule);
    }

    protected void addParent(ProcessorNode aInParent)
    {
        super.addParent(aInParent);
        scope = DefinitionScope.get(aInParent.getName());
    }

    private DefinitionScope scope = null;
}