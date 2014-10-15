package org.opendaylight.opflex.genie.content.model.mownership;

import org.opendaylight.opflex.genie.engine.model.Cat;

/**
 * Created by midvorki on 9/27/14.
 */
public class MOwnershipRule extends MOwnershipComponent
{
    protected MOwnershipRule(Cat aInCat, MOwnershipComponent aInParent, String aInName, DefinitionScope aInDefScope)
    {
        super(aInCat,aInParent,aInName, aInDefScope);
    }
}
