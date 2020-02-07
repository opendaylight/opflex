package org.opendaylight.opflex.genie.content.model.mrelator;

import java.util.Collection;
import java.util.LinkedList;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.module.Module;
import org.opendaylight.opflex.genie.engine.model.Cardinality;
import org.opendaylight.opflex.genie.engine.model.RelatorCat;

/**
 * Created by midvorki on 10/2/14.
 */
public class MRelationshipClass extends MClass
{
    public static final RelatorCat SOURCE_CAT = RelatorCat.getCreate("mclass:reln-source", Cardinality.SINGLE);

    public MRelationshipClass(
            Module aInModule,
            String aInLName,
            MRelationship aInReln)
    {
        super(aInModule, aInLName);
        setConcrete(true);
        addSource(aInReln.getSourceClassGName());
        reln.add(aInReln);
    }

    public void addTargetRelationship(MRelationship aIn)
    {
        reln.add(aIn);
    }

    public Collection<MClass> getTargetClasses(boolean aInExpandToConcrete)
    {
        LinkedList<MClass> lRet = new LinkedList<>();
        for (MRelationship lRel : reln)
        {
            MClass lClass = lRel.getTargetClass();
            if (lClass.isConcrete())
            {
                lRet.add(lClass);
            }
            else if (aInExpandToConcrete)
            {

                Collection<MClass> lSubclasses = new LinkedList<>();
                lClass.getSubclasses(
                        lSubclasses,
                        false, // boolean aInIsDirectOnly,
                        true //boolean aInIsConcreteOnly
                        );
                lRet.addAll(lSubclasses);
            }
            else
            {
                lRet.add(lRel.getTargetClass());
            }
        }
        return lRet;
    }

    private void addSource(String aInClassGName)
    {
        SOURCE_CAT.add(MY_CAT, getGID().getName(), MY_CAT, aInClassGName);
    }

    private final Collection<MRelationship>  reln = new LinkedList<>();
}
