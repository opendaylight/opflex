package org.opendaylight.opflex.genie.content.parse.ptype;

import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.content.model.mtype.MTypeHint;
import org.opendaylight.opflex.genie.content.model.mtype.TypeInfo;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/21/14.
 */
public class PHintNode
        extends ParseNode
{
    /**
     * Constructor
     */
    public PHintNode(String aInName)
    {
        super(aInName);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        MTypeHint lHint = new MTypeHint((MType)aInParentItem, TypeInfo.get(aInData.getNamedValue(Strings.NAME,null,true)));
        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE,lHint);
    }
}
