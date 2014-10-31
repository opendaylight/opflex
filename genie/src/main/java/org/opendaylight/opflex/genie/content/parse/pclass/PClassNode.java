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
        //boolean lIsAbstract = aInData.checkFlag(Strings.ABSTRACT);
        String lIsAbstract = aInData.getNamedValue(Strings.ABSTRACT, null, false);
        String lIsConcrete = aInData.getNamedValue(Strings.CONCRETE, null, false);

        Boolean lAbstractnessSpec =
                Strings.isEmpty(lIsAbstract) ?
                        null :
                        (("yes".equalsIgnoreCase(lIsAbstract) ||
                          Strings.CONCRETE.equalsIgnoreCase(lIsAbstract) ||
                          !"no".equalsIgnoreCase(lIsAbstract)));

        Boolean lConcretenessSpec =
                Strings.isEmpty(lIsConcrete) ?
                        null :
                        (("yes".equalsIgnoreCase(lIsConcrete) ||
                          Strings.CONCRETE.equalsIgnoreCase(lIsConcrete) ||
                          !"no".equalsIgnoreCase(lIsConcrete)));

        Boolean lConcreteness = null == lConcretenessSpec ?
                                    null == lAbstractnessSpec ?
                                            null :
                                            !lAbstractnessSpec :
                                    lConcretenessSpec;

        MClass lClass = MClass.defineClass((Module) aInParentItem, lName, lConcreteness);
        // ADD SUPERCLASS IF ONE IS DECLARED
        String lSuper = aInData.getNamedValue(Strings.SUPER,null,false);
        if (!Strings.isEmpty(lSuper))
        {
            lClass.addSuperclass(lSuper);
        }
        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE,lClass);
    }
}
