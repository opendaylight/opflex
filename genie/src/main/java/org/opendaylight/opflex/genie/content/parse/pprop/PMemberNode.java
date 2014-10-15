package org.opendaylight.opflex.genie.content.parse.pprop;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.mprop.MProp;
import org.opendaylight.opflex.genie.content.model.mprop.PropAction;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/23/14.
 */
public class PMemberNode
        extends ParseNode
{
    /**
     * Constructor
     */
    public PMemberNode(String aInName)
    {
        super(aInName);
        action = PropAction.get(aInName);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        String lName = aInData.getNamedValue(Strings.NAME,null, true);
        MProp lProp = new MProp((MClass)aInParentItem,lName,action);
        String lGroup = aInData.getNamedValue("group",action.isDefine() ? Strings.DEFAULT : null, false);
        if (!Strings.isEmpty(lGroup))
        {
            lProp.setGroup(lGroup);
        }
        String lType = aInData.getNamedValue(Strings.TYPE,null,false);
        if (!Strings.isEmpty(lType))
        {
            lProp.addType(lType);
        }
        return null;
    }

    private final PropAction action;
}
