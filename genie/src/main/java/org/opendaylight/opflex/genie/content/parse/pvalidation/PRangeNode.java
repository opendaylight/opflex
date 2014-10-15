package org.opendaylight.opflex.genie.content.parse.pvalidation;

import org.opendaylight.opflex.genie.content.model.mvalidation.*;
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
public class PRangeNode
        extends ParseNode
{
    public PRangeNode(String aInName)
    {
        super(aInName);
        action = ValidatorAction.get(aInName);
    }

    protected void addParent(ProcessorNode aInParent)
    {
        super.addParent(aInParent);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        MRange lRange = new MRange(
                (MValidator)aInParentItem,
                aInData.getNamedValue(Strings.NAME,Strings.DEFAULT,true), action);

        addConstraints(aInData,lRange);
        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE,lRange);
    }

    private void addConstraints(Node aInData, MRange aInRange)
    {
        for (ConstraintValueType lConstrType : ConstraintValueType.values())
        {
            addConstraint(aInData,aInRange,lConstrType);
        }
    }

    private void addConstraint(Node aInData, MRange aInRange, ConstraintValueType aInType)
    {
        String lConstr = aInData.getNamedValue(aInType.getName(),null, false);
        if (!Strings.isEmpty(lConstr))
        {
            new MConstraintValue(aInRange, lConstr, aInType);
        }
    }
    private final ValidatorAction action;
}
