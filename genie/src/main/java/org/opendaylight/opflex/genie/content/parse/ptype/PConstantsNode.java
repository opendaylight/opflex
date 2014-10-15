package org.opendaylight.opflex.genie.content.parse.ptype;

import org.opendaylight.opflex.genie.content.model.mtype.DefinedIn;
import org.opendaylight.opflex.genie.content.model.mtype.MConstants;
import org.opendaylight.opflex.genie.content.model.mtype.MLanguageBinding;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;

/**
 * Created by midvorki on 7/28/14.
 */
public class PConstantsNode
        extends ParseNode
{
    /**
     * Constructor
     */
    public PConstantsNode(String aInName)
    {
        super(aInName);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        return new Pair<ParseDirective, Item>(
            ParseDirective.CONTINUE,
            new MConstants((MLanguageBinding) aInParentItem,
                           DefinedIn.get(aInData.getNamedValue("defined-in", null, false)),
                           aInData.getNamedValue("prefix", null, false),
                           aInData.getNamedValue("suffix", null, false)));
    }

}