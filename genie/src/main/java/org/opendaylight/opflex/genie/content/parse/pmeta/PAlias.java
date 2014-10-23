package org.opendaylight.opflex.genie.content.parse.pmeta;

import org.opendaylight.opflex.genie.content.model.mmeta.MNode;
import org.opendaylight.opflex.genie.content.model.mmeta.MNodeAlias;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 10/22/14.
 */
public class PAlias
        extends ParseNode
{
    /**
     * Constructor
     */
    public PAlias(String aInName)
    {
        super(aInName, false);
    }

    /**
     * checks if the property is supported by this node. this overrides behavior to always return true
     * @param aInName name of the property
     * @return always returns true
     */
    public boolean hasProp(String aInName)
    {
        return true;
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        MNodeAlias lNA = new MNodeAlias((MNode)aInParentItem,aInData.getNamedValue(Strings.NAME, null, true));
        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE, lNA);
    }
}
