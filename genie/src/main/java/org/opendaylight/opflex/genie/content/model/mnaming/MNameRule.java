package org.opendaylight.opflex.genie.content.model.mnaming;

import java.util.Collection;
import java.util.LinkedList;

import org.opendaylight.opflex.genie.engine.model.*;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/10/14.
 *
 * Name rule is a containment specific name rule, a model item that identifies: a) how an object
 * is named in a context of one parent b) how an object is named for the rest of the containment
 * context not explicitly specified
 */
public class MNameRule extends Item
{
    /**
     * category identifying all containment specific name rules
     */
    public static final Cat MY_CAT = Cat.getCreate("mnamer:rule");

    /**
     * Constructor
     * @param aInNamer a namer rule under which this containment specific rule is created
     * @param aInContainerClassOrAny specifies specific container class or "any"/null, in case general rule is desired.
     */
    public MNameRule(MNamer aInNamer, String aInContainerClassOrAny)
    {
        super(MY_CAT,
              aInNamer,
              Strings.isAny(aInContainerClassOrAny) ?
                NameRuleScope.ANY.getName() :
                aInContainerClassOrAny
                );
    }

    /**
     * namer rule accessor
     * @return namer rule that contains this item
     */
    public MNamer getNamer()
    {
        return (MNamer) getParent();
    }

    public String getNextComponentName()
    {
        return "" + (++componentCount);
    }

    public void getComponents(Collection<MNameComponent> aOut)
    {
        LinkedList<Item> lChildren = new LinkedList<>();
        getChildItems(MNameComponent.MY_CAT,lChildren);
        for (Item lIt : lChildren)
        {
            aOut.add((MNameComponent) lIt);
        }
    }

    public Collection<MNameComponent> getComponents()
    {
        LinkedList<MNameComponent> lComps = new LinkedList<>();
        getComponents(lComps);
        return lComps;
    }

    private int componentCount = 0;
}
