package org.opendaylight.opflex.genie.content.parse.pclass;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.module.Module;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.report.Severity;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/23/14.
 */
public class PClassNode
        extends ParseNode
{
    /**
     * Constructor
     */
    public PClassNode(String aInName)
    {
        super(aInName);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        if (null == aInParentItem)
        {
            aInParentItem = Module.get(aInData.getParent().getNamedValue(Strings.NAME, null, true), true);

            Severity.WARN.report(toString(),"parsing", "null parent for: " + aInData, "assuming autoretrieved: " + aInParentItem);
        }
        // GET NAME
        String lName = aInData.getNamedValue(Strings.NAME,null,true);


        // DETERMINE IF THIS CLASS IS CONCRETE
        boolean lIsAbstract = aInData.checkFlag(Strings.ABSTRACT);

        boolean lIsConcrete = (!lIsAbstract) &&
                              aInData.checkFlag(Strings.CONCRETE);
        // CREATE A CLASS
        MClass lClass = new MClass(
                (Module) aInParentItem,
                lName,
                lIsConcrete);


        // ADD SUPERCLASS IF ONE IS DECLARED
        String lSuper = aInData.getNamedValue(Strings.SUPER,null,false);
        if (!Strings.isEmpty(lSuper))
        {
            lClass.addSuperclass(lSuper);
        }
        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE,lClass);
    }

}
