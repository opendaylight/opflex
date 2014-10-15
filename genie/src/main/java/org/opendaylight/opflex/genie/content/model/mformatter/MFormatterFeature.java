package org.opendaylight.opflex.genie.content.model.mformatter;

import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;

/**
 * Created by midvorki on 7/24/14.
 */
public class MFormatterFeature
        extends Item
{
    /**
     * category of this item
     */
    public static final Cat MY_CAT = Cat.getCreate("formatter:feature");


    /**
     * Constructor
     * @param aInLName name of the feature
     */
    public MFormatterFeature(MFormatterDomain aInDomain, String aInLName)
    {
        super(MY_CAT, aInDomain, aInLName);
    }

    public MFormatterDomain getDomain()
    {
        return (MFormatterDomain) getParent();
    }
}
