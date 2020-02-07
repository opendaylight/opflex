package org.opendaylight.opflex.genie.content.model.mvalidation;

import org.opendaylight.opflex.genie.engine.model.Cat;

/**
 * Created by midvorki on 7/10/14.
 */
public class MRange
        extends MConstraint
{
    public static final Cat MY_CAT = Cat.getCreate("mvalidator:mrange");

    public MRange(MValidator aInParent, String aInName)
    {
        super(MY_CAT, aInParent, aInName);
    }

}
