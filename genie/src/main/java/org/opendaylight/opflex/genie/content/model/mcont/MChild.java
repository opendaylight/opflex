package org.opendaylight.opflex.genie.content.model.mcont;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.engine.model.Cardinality;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.RelatorCat;

/**
 * Created by dvorkinista on 7/8/14.
 *
 * Specifies containment rule between two classes from the perspective of a parent. The parent class is represented by
 * parent item MContainer, and the child class is represented by this class, MChild.
 *
 * This rule identifies that the child (represented by mContained) class can be contained on the managed information tree
 * by the parent class (represented by MParent).
 *
 * This class is inverse of MContained.
 *
 * Containment rule holds directives on lifecycle control as well as directive affecting relative naming.
 *
 * Containment relationships can't be instantiated via direct construction, use MContained.addRule(...) method.
 */
public class MChild extends MContRuleItem
{
    public static final Cat MY_CAT = Cat.getCreate("mcont:mchild");
    public static final RelatorCat TARGET_CAT = RelatorCat.getCreate("mcont:mchild:childref", Cardinality.SINGLE);

    public MChild(
            MContainer aInParent,
            String aInChildGname)
    {
        super(MY_CAT, aInParent, TARGET_CAT, aInChildGname);
    }

    public MContainer getContainer()
    {
        return (MContainer) getParent();
    }

    public MClass getContainerClass()
    {
        return getContainer().getTarget();
    }

    public MContained getMContained()
    {
        return MContained.get(getLID().getName());
    }

    public MParent getMParent()
    {
        return getMContained().getMParent(getContainer().getLID().getName());
    }

    public void validateCb()
    {
        super.validateCb();
        /**Severity.WARN.report(toString(),"validate", "",
                             "\n\tcontainer: " + getContainer() + "\n" +
                             "\tcontainer class: " + getContainerClass() + "\n" +
                             "\tcontained: " + getMContained() + "\n" +
                             "\tmparent: " + getMParent());**/
    }

}
