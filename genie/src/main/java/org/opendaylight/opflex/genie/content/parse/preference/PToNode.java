package org.opendaylight.opflex.genie.content.parse.preference;

import org.opendaylight.opflex.genie.content.model.mrelator.MRelated;
import org.opendaylight.opflex.genie.content.model.mrelator.MRelator;
import org.opendaylight.opflex.genie.content.model.mrelator.PointCardinality;
import org.opendaylight.opflex.genie.content.model.mrelator.RelatorType;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 8/1/14.
 */
public class PToNode extends ParseNode
{
    public PToNode(String aInName)
    {
        super(aInName);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
//        PDependencyNode lContainedParseNode = (PDependencyNode) getParent();
        RelatorType lRelatorType = ((PDependencyNode) getParent()).getType();
        String lRelatorName = aInData.getParent().getNamedValue(Strings.NAME,"Reln",true);
        String lRelatingClassName = aInParentItem.getLID().getName();
        PointCardinality lRelatingCardinality =
                PointCardinality.get(aInData.getParent().getNamedValue(Strings.CARDINALITY, "many", true));

        PointCardinality lRelatedCardinality = PointCardinality.get(
                aInData.getNamedValue(Strings.CARDINALITY, "single", true));

        String lRelatedClassName =
                aInData.getNamedValue(Strings.CLASS, null, true);

        Pair<MRelator, MRelated> lRule = MRelator.addRule(
                lRelatorType, lRelatorName, lRelatingClassName,lRelatingCardinality, lRelatedClassName, lRelatedCardinality) ;

//        Severity.WARN.report(this.toString(),"","","CONT RULE ADDED: " + lRule);
        MRelated lRelated = lRule.getSecond();

        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE, lRelated);
    }
}
