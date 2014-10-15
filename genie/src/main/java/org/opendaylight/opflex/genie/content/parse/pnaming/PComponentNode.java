package org.opendaylight.opflex.genie.content.parse.pnaming;

import org.opendaylight.opflex.genie.content.model.mcont.MContained;
import org.opendaylight.opflex.genie.content.model.mcont.MParent;
import org.opendaylight.opflex.genie.content.model.mnaming.MNameComponent;
import org.opendaylight.opflex.genie.content.model.mnaming.MNameRule;
import org.opendaylight.opflex.genie.content.model.mnaming.MNamer;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.model.ProcessorNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;

/**
 * Created by midvorki on 8/1/14.
 */
public class PComponentNode extends ParseNode
{
    public PComponentNode(String aInName)
    {
        super(aInName);
    }

    protected void addParent(ProcessorNode aInParent)
    {
        super.addParent(aInParent);
        isPartOfContainmentDef = aInParent instanceof org.opendaylight.opflex.genie.content.parse.pcontainment.PParentNode;
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        // CHECK IF THE NAMING RULE IS DEFINED AS PART OF CONTAINMENT,
        // IF IT IS, WE NEED TO ALLOCATE THE NAME RULE THAT SHADOWS THE CONTAINMENT RULE
        if (isPartOfContainmentDef)
        {
            MParent lParent = (MParent) aInParentItem;
            MContained lContd = lParent.getContained();
            MNamer lNamer = MNamer.get(lContd.getLID().getName(), true);
            MNameRule lNr = lNamer.getNameRule(lParent.getLID().getName(), true);
            aInParentItem = lNr;
        }
        return new Pair<ParseDirective, Item>(
                ParseDirective.CONTINUE,
                new MNameComponent((MNameRule)aInParentItem,
                                   aInData.getNamedValue("prefix", null, false),
                                   aInData.getNamedValue("member", null, false)
                                   ));
    }

    private boolean isPartOfContainmentDef = false;
}