package org.opendaylight.opflex.genie.content.parse.pcontainment;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.mcont.MContained;
import org.opendaylight.opflex.genie.content.model.mcont.MParent;
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
        PContainedNode lContainedParseNode = (PContainedNode) getParent();

        String lContainedClassName = aInParentItem.getLID().getName();
        String lContainerClassName = lContainedParseNode.isRoot() ?
                                            MClass.ROOT_CLASS_GNAME :
                                            aInData.getNamedValue(Strings.CLASS, null, true);

        Pair<MContained, MParent> lRule = MContained.addRule(lContainerClassName,lContainedClassName);

//        Severity.WARN.report(this.toString(),"","","CONT RULE ADDED: " + lRule);
        MParent lContainer = lRule.getSecond();

        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE, lContainer);
    }
}
