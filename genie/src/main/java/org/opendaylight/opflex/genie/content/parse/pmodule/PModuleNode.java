package org.opendaylight.opflex.genie.content.parse.pmodule;

import org.opendaylight.opflex.genie.content.model.module.Module;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;

/**
 * Created by midvorki on 7/21/14.
 */
public class PModuleNode
        extends ParseNode
{
    /**
     * Constructor
     */
    public PModuleNode(String aInName)
    {
        super(aInName);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        //System.out.println("----------->" + this + ".beginCb(" + aInData + ", " + aInParentItem + ")");
        return new Pair<ParseDirective, Item>(
                    ParseDirective.CONTINUE,
                    Module.get(aInData.getNamedValue("name",null, true), true));
    }
}