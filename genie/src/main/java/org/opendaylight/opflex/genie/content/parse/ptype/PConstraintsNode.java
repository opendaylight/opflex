package org.opendaylight.opflex.genie.content.parse.ptype;

import org.opendaylight.opflex.genie.content.model.mtype.MConstraints;
import org.opendaylight.opflex.genie.content.model.mtype.MLanguageBinding;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;

/**
 * Created by midvorki on 7/28/14.
 */
public class PConstraintsNode
        extends ParseNode
{
    /**
     * Constructor
     */
    public PConstraintsNode(String aInName)
    {
        super(aInName);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        return new Pair<>(
            ParseDirective.CONTINUE,
            new MConstraints((MLanguageBinding) aInParentItem,
                aInData.getNamedValue("use-type", null, false)
            ));
    }

}