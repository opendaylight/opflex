package org.opendaylight.opflex.genie.content.parse.pcontainment;

import org.opendaylight.opflex.genie.content.model.mclass.DefinitionScope;
import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.mcont.MContained;
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
public class PContainedNode extends ParseNode
{
    public PContainedNode(String aInName)
    {
        super(aInName);
        isRoot = Strings.ROOT.equalsIgnoreCase(aInName);
    }

    protected void addParent(ProcessorNode aInParent)
    {
        super.addParent(aInParent);
        scope = DefinitionScope.get(aInParent.getName());
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        String lContainedClass = DefinitionScope.CLASS == scope ?
                                            aInParentItem.getGID().getName() :
                                            aInData.getNamedValue(Strings.CLASS,null, true);

        MContained lContained = isRoot ?
                                MContained.addRule(MClass.ROOT_CLASS_GNAME, lContainedClass).getFirst() :
                                MContained.get(lContainedClass, true);


//        Severity.WARN.report(toString(),"","", (isRoot ? "ROOT " : "NON-ROOT ") + scope + " CONTAINED RULE ADDED: " + lContained);

        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE, lContained);
    }

    public boolean isRoot()
    {
        return isRoot;
    }

    public DefinitionScope getScope()
    {
        return scope;
    }

    private DefinitionScope scope;
    private boolean isRoot;
}
