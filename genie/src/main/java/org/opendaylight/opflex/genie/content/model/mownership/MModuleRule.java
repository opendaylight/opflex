package org.opendaylight.opflex.genie.content.model.mownership;

import java.util.LinkedList;

import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;

/**
 * Created by midvorki on 9/27/14.
 */
public class MModuleRule extends MOwnershipRule
{
    public static final Cat MY_CAT = Cat.getCreate("mowner:module");
    public MModuleRule(MOwner aInParent, String aInNameOrAll)
    {
        super(MY_CAT,aInParent,aInNameOrAll,DefinitionScope.OWNER);
    }

    public void initTargets()
    {
        LinkedList<Item> lIts = new LinkedList<>();

        getChildItems(MClassRule.MY_CAT, lIts);
        for (Item lIt : lIts)
        {
            MClassRule lRule = (MClassRule) lIt;
            lRule.initTargets();
        }
    }
}
