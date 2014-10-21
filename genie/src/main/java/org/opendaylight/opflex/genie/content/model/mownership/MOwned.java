package org.opendaylight.opflex.genie.content.model.mownership;

import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;

/**
 * Created by midvorki on 9/27/14.
 */
public class MOwned extends Item
{
    public static final Cat MY_CAT = Cat.getCreate("mowned");

    public MOwned(Item aInParent, MOwner aInOwner, MClassRule aInRule)
    {
        super(MY_CAT,aInParent,aInOwner.getLID().getName());
        owner = aInOwner;
        rule = aInRule;
    }

    public MOwner getOwner()
    {
        return owner;
    }

    public MClassRule getRule() { return rule; }

    public DefinitionScope getScope() { return rule.getDefinitionScope(); }


    private final MOwner owner;
    private final MClassRule rule;
}
