package org.opendaylight.opflex.genie.content.parse.pvalidation;

import org.opendaylight.opflex.genie.content.model.mvalidation.MValidator;
import org.opendaylight.opflex.genie.content.model.mvalidation.ValidatorAction;
import org.opendaylight.opflex.genie.content.model.mvalidation.ValidatorScope;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.model.ProcessorNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/22/14.
 */
public class PValidateNode
        extends ParseNode
{
    public PValidateNode(String aInName)
    {
        super(aInName);
        action = ValidatorAction.get(aInName);
    }

    protected void addParent(ProcessorNode aInParent)
    {
        super.addParent(aInParent);
        scope = ValidatorScope.get(aInParent.getName());
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        MValidator lVal = new MValidator(aInParentItem,aInData.getNamedValue(Strings.NAME,Strings.DEFAULT,true), scope,action);
        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE,lVal);
    }

    private final ValidatorAction action;
    private ValidatorScope scope;

}
