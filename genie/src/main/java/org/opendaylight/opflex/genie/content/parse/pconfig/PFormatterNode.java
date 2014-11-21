package org.opendaylight.opflex.genie.content.parse.pconfig;

import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.genie.engine.proc.Config;

/**
 * Created by midvorki on 10/22/14.
 */
public class PFormatterNode
        extends ParseNode
{
    /**
     * Constructor
     */
    public PFormatterNode()
    {
        super("formatter", false);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        Config.setFormatterRelPath(aInData.getNamedValue("path", null, true), aInData
                .getNamedValue("filetype", ".meta", true));

        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE,aInParentItem);
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
}