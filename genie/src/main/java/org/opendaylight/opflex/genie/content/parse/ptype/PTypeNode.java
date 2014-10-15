package org.opendaylight.opflex.genie.content.parse.ptype;

import org.opendaylight.opflex.genie.content.model.module.Module;
import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/21/14.
 *
 * Parses type nodes
 *
 * module<name> <\br></\br>
 */
public class PTypeNode
        extends ParseNode
{
    public static final String PRIMITIVE = "primitive";

    /**
     * Constructor
     */
    public PTypeNode(String aInName)
    {
        super(aInName);
        isPrimitive = PRIMITIVE.equalsIgnoreCase(aInName);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        //System.out.println("----------->" + this + ".beginCb(" + aInData + ", " + aInParentItem + ")");
        MType lType = new MType((Module) aInParentItem, aInData.getNamedValue(Strings.NAME,null,true), isPrimitive);
        lType.addSupertype(aInData.getNamedValue(Strings.SUPER, null, !isPrimitive));
        lType.addLiketype(aInData.getNamedValue("like", null, false));
        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE,lType);
    }

    public boolean isPrimitive()
    {
        return isPrimitive;
    }

    private final boolean isPrimitive;
}
