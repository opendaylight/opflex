package org.opendaylight.opflex.genie.content.model.mownership;

import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;

/**
 * Created by midvorki on 9/27/14.
 */
public class MOwned extends Item
{
    public static final Cat MY_CAT = Cat.getCreate("mowned");

    public MOwned(Item aInParent, String aInOwner)
    {
        super(MY_CAT,aInParent,aInOwner);
    }

    public MOwner getOwner()
    {
        return MOwner.get(getLID().getName(),false);
    }
}
