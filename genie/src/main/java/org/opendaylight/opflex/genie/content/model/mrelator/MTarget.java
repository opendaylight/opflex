package org.opendaylight.opflex.genie.content.model.mrelator;

import java.util.Collection;
import java.util.LinkedList;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.engine.model.Cardinality;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.RelatorCat;

/**
 * Created by midvorki on 8/5/14.
 */
public class MTarget extends MRelatorRuleItem
{
    public static final Cat MY_CAT = Cat.getCreate("rel:target");
    public static final RelatorCat TARGET_CAT = RelatorCat.getCreate("rel:target:targetref", Cardinality.SINGLE);

    MTarget(
            MRelator aInSource,
            String aInTargetGname)
    {
        super(MY_CAT, aInSource, TARGET_CAT, aInTargetGname);
    }

    public MRelator getRelator()
    {
        return (MRelator) getParent();
    }

    public MClass getRelatorClass()
    {
        return getRelator().getTarget();
    }

    public MRelated getMRelated()
    {
        return MRelated.get(getLID().getName());
    }

    public MSource getMSource()
    {
        return getMRelated().getMSource(getRelator().getLID().getName());
    }

    public MRelationship getRelationship(String aInName)
    {
        return (MRelationship) getChildItem(MRelationship.MY_CAT, aInName);
    }

    public void getRelationships(Collection<MRelationship> aOut)
    {
        LinkedList<Item> lRels = new LinkedList<Item>();
        getChildItems(MRelationship.MY_CAT, lRels);
        for (Item lThis : lRels)
        {
            aOut.add((MRelationship) lThis);
        }
    }

    public void validateCb()
    {
        super.validateCb();

        /*Severity.WARN.report(toString(),"validate", "",
                             "\n\trelator: " + getRelator() + "\n" +
                             "\trelator class: " + getRelatorClass() + "\n" +
                             "\trelated: " + getMRelated() + "\n" +
                             "\tmsource: " + getMSource());*/
    }
}
