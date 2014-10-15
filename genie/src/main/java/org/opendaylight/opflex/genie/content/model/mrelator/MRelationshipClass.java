package org.opendaylight.opflex.genie.content.model.mrelator;

import java.util.Collection;
import java.util.LinkedList;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.module.Module;
import org.opendaylight.opflex.genie.engine.model.Cardinality;
import org.opendaylight.opflex.genie.engine.model.Relator;
import org.opendaylight.opflex.genie.engine.model.RelatorCat;
import org.opendaylight.opflex.modlan.report.Severity;

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
        super(aInModule, aInLName, true);
        addSource(aInReln.getSourceClassGName());
        reln.add(aInReln);
    }

    public void addTargetRelationship(MRelationship aIn)
    {
        reln.add(aIn);
    }

    public Collection<MRelationship> getRelationships()
    {
        return reln;
    }

    public MClass getSourceClass()
    {
        Relator lRel = SOURCE_CAT.getRelator(getGID().getName());
        if (null == lRel)
        {
            Severity.DEATH.report("relationshop class", "retrieval of source class", "source class not found", "no relationship registered");
        }
        MClass lClass = (MClass) (null == lRel ? null : lRel.getToItem());
        if (null == lClass)
        {
            Severity.DEATH.report("relationshop class", "retrieval of source class", "source class not found", "bad relationship");
        }
        return lClass;
    }

    public Collection<MClass> getTargetClasses(boolean aInExpandToConcrete)
    {
        LinkedList<MClass> lRet = new LinkedList<MClass>();
        for (MRelationship lRel : reln)
        {
            MClass lClass = lRel.getTargetClass();
            if (lClass.isConcrete())
            {
                lRet.add(lClass);
            }
            else if (aInExpandToConcrete)
            {

                Collection<MClass> lSubclasses = new LinkedList<MClass>();
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



    private Collection<MRelationship> reln = new LinkedList<MRelationship>();
}
