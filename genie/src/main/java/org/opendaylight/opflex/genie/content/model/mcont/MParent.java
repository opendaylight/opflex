package org.opendaylight.opflex.genie.content.model.mcont;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.engine.model.Cardinality;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.RelatorCat;

/**
 * Created by dvorkinista on 7/8/14.
 *
 * Specifies containment rule between two classes from the perspective of a child. The child class is represented by
 * parent item MContained, and the parent class is represented by this class, MParent.
 *
 * This rule identifies that the child (represented by mContained) class can be contained on the managed information tree
 * by the parent class (represented by MParent).
 *
 * This class is inverse of MContainer.
 *
 * Containment rule holds directives on lifecycle control as well as directive affecting relative naming.
 *
 * Containment relationships can't be instantiated via direct construction, use MContained.addRule(...) method.
 */
public class MParent extends MContRuleItem
{
    public static final Cat MY_CAT = Cat.getCreate("mcont:mparent");
    public static final RelatorCat TARGET_CAT = RelatorCat.getCreate("mcont:mparent:parentref", Cardinality.SINGLE);

    MParent(
            MContained aInChild,
            String aInParentGname)
    {
        super(MY_CAT, aInChild, TARGET_CAT, aInParentGname);
    }

    public MContained getContained()
    {
        return (MContained) getParent();
    }

    public MClass getContainedClass()
    {
        return getContained().getTarget();
    }

    public MContainer getMContainer()
    {
        return MContainer.get(getLID().getName());
    }

    public MChild getMChild()
    {
        return getMContainer().getMChild(getContained().getLID().getName());
    }

    public void validateCb()
    {
        super.validateCb();

        /*Severity.WARN.report(toString(),"validate", "",
                             "\n\tcontained: " + getContained() + "\n" +
                             "\tcontained class: " + getContainedClass() + "\n" +
                             "\tcontainer: " + getMContainer() + "\n" +
                             "\tmchild: " + getMChild());*/
    }
}
