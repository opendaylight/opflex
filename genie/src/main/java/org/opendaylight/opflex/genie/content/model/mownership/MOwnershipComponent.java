package org.opendaylight.opflex.genie.content.model.mownership;

import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 9/27/14.
 */
public class MOwnershipComponent extends Item
{
    protected MOwnershipComponent(Cat aInCat, Item aInParent, String aInName, DefinitionScope aInDefScope)
    {
        super(aInCat,aInParent,aInName);
        definitionScope = aInDefScope;
        isWildCard = Strings.isEmpty(aInName) || Strings.isWildCard(aInName);
    }

    public DefinitionScope getDefinitionScope()
    {
        return definitionScope;
    }

    public boolean isWildCard()
    {
        return isWildCard;
    }

    private DefinitionScope definitionScope;
    private boolean isWildCard;
}
