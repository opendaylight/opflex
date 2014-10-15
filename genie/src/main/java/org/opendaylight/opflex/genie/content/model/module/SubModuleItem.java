package org.opendaylight.opflex.genie.content.model.module;

import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;

/**
 * Created by dvorkinista on 7/6/14.
 */
public class SubModuleItem extends Item
{
    protected SubModuleItem(
            Cat aInCat,
            Item aInParent,
            String aInLName
            )
    {
        super(aInCat, aInParent, aInLName);
    }

    public Module getModule()
    {
        return Module.getModule(this);
    }
}
