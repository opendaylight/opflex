package org.opendaylight.opflex.genie.content.model.mtype;

import org.opendaylight.opflex.genie.content.model.module.SubModuleItem;
import org.opendaylight.opflex.genie.engine.model.Cat;

/**
 * Created by dvorkinista on 7/7/14.
 */
public class SubTypeItem extends SubModuleItem
{
    protected SubTypeItem(Cat aInCat,MType aInParent,String aInLName)
    {
        super(aInCat, aInParent, aInLName);
    }

    protected SubTypeItem(Cat aInCat,SubTypeItem aInParent,String aInLName)
    {
        super(aInCat, aInParent, aInLName);
    }

    public MType getMType()
    {
        return MType.getMType(this);
    }
}
