package org.opendaylight.opflex.genie.content.parse.pconst;

import org.opendaylight.opflex.genie.content.model.mconst.*;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.model.ProcessorNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/21/14.
 */
public class PConstNode
        extends ParseNode
{
    /**
     * Constructor
     */
    public PConstNode(String aInName)
    {
        super(aInName);
        action = ConstAction.get(aInName);
    }

    protected void addParent(ProcessorNode aInParent)
    {
        super.addParent(aInParent);
        scope = ConstScope.get(aInParent.getName());
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        String lName = (action == ConstAction.DEFAULT) ?
                            Strings.DEFAULT :
                            (action == ConstAction.EXCLUSIVE) ?
                                    Strings.EXCLUSIVE :
                                    aInData.getNamedValue(Strings.NAME, null, true);

        String lIndirectionTarget = (action == ConstAction.DEFAULT || action == ConstAction.EXCLUSIVE) ?
                aInData.getNamedValue(Strings.NAME, null, true) :
                aInData.getNamedValue(Strings.TARGET, null, action.hasExplicitIndirection());

        String lValue = aInData.getNamedValue(Strings.VALUE, null, action.hasMandatoryValue());

        MConst lConst = factory(action, aInParentItem, lName, lIndirectionTarget, lValue);

        if (ConstAction.EXCLUSIVE == action)
        {
            // FOR EXCLUSIVE, NEED TO
            factory(ConstAction.DEFAULT, aInParentItem, Strings.DEFAULT, lIndirectionTarget, null);
        }

        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE, lConst);
    }

    private MConst factory(ConstAction aInAction, Item aInParentItem, String aInName, String aInIndTarget, String aInValue)
    {
        MConst lConst = new MConst(aInParentItem, aInName, aInAction, scope);

        if (!Strings.isEmpty(aInIndTarget))
        {
            new MIndirection(lConst, aInIndTarget);
        }
        if (!Strings.isEmpty(aInValue))
        {
            new MValue(lConst, aInValue);
        }
        return lConst;
    }

    private ConstAction action;
    private ConstScope scope = null;
}
