package org.opendaylight.opflex.genie.content.parse.preference;

import org.opendaylight.opflex.genie.content.model.mclass.DefinitionScope;
import org.opendaylight.opflex.genie.content.model.mrelator.MRelator;
import org.opendaylight.opflex.genie.content.model.mrelator.RelatorType;
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
public class PDependencyNode extends ParseNode
{
    public PDependencyNode(String aInName)
    {
        super(aInName);
        type = RelatorType.get(aInName);
    }

    protected void addParent(ProcessorNode aInParent)
    {
        super.addParent(aInParent);
        scope = DefinitionScope.get(aInParent.getName());
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        String lRelatingClass = DefinitionScope.CLASS == scope ?
                aInParentItem.getGID().getName() :
                aInData.getNamedValue(Strings.CLASS,null, true);

        MRelator lRel = MRelator.get(lRelatingClass,true);


//        Severity.WARN.report(toString(),"","", (isRoot ? "ROOT " : "NON-ROOT ") + scope + " CONTAINED RULE ADDED: " + lContained);

        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE, lRel);
    }

    public DefinitionScope getScope()
    {
        return scope;
    }

    public RelatorType getType() { return type; }

    private DefinitionScope scope;
    private RelatorType type;
}
