package org.opendaylight.opflex.genie.content.parse.pnaming;

import org.opendaylight.opflex.genie.content.model.mclass.DefinitionScope;
import org.opendaylight.opflex.genie.content.model.mnaming.MNamer;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.model.ProcessorNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 8/1/14.
 */
public class PNamedNode extends ParseNode
{
    public PNamedNode(String aInName)
    {
        super(aInName);
    }

    protected void addParent(ProcessorNode aInParent)
    {
        super.addParent(aInParent);
        scope = DefinitionScope.get(aInParent.getName());
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE,
                                              MNamer.get(
                                                      (DefinitionScope.CLASS == scope ?
                                                              aInParentItem.getGID().getName() :
                                                              aInData.getNamedValue(Strings.CLASS,
                                                                                    aInData.getNamedValue(Strings.NAME, null, false),
                                                                                    true)),
                                                      true));
    }

    private DefinitionScope scope = DefinitionScope.NONE;

}