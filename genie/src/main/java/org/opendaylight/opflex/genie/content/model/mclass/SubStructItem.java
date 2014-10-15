package org.opendaylight.opflex.genie.content.model.mclass;

import org.opendaylight.opflex.genie.content.model.module.SubModuleItem;
import org.opendaylight.opflex.genie.engine.model.Cat;

/**
 * Created by dvorkinista on 7/7/14.
 */
public class SubStructItem extends SubModuleItem
{
    protected SubStructItem(Cat aInCat,MClass aInParent, String aInLName)
    {
        super(aInCat, aInParent, aInLName);
    }

    public MClass getMClass()
    {
        return MClass.getClass(this);
    }
}