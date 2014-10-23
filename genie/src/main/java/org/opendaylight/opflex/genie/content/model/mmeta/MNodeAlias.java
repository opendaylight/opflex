package org.opendaylight.opflex.genie.content.model.mmeta;

import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNodePropType;

/**
 * Created by midvorki on 10/22/14.
 */
public class MNodeAlias
        extends Item
{
    /**
     * category of this item
     */
    public static final Cat MY_CAT = Cat.getCreate("parser:meta:alias");

    public MNodeAlias(MNode aInParent, String aInName)
    {
        super(MY_CAT, aInParent, aInName);
    }
}
