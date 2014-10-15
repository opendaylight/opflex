package org.opendaylight.opflex.genie.content.parse.ptype;

import org.opendaylight.opflex.genie.content.model.mtype.Language;
import org.opendaylight.opflex.genie.content.model.mtype.MLanguageBinding;
import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.content.model.mtype.PassBy;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/21/14.
 */
public class PLanguageNode
        extends ParseNode
{
    /**
     * Constructor
     */
    public PLanguageNode(String aInName)
    {
        super(aInName);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        MLanguageBinding lLang = new MLanguageBinding(
                (MType)aInParentItem,
                Language.get(aInData.getNamedValue(Strings.NAME, null, true)),
                aInData.getNamedValue("syntax", null, false),
                aInData.getNamedValue("object-syntax", null, false),
                PassBy.get(aInData.getNamedValue("pass-by", null, false)),
                aInData.checkFlag("pass-const"),
                aInData.getNamedValue("include", null, false),
                aInData.getNamedValue("like", null, false));

        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE, lLang);
    }

}
