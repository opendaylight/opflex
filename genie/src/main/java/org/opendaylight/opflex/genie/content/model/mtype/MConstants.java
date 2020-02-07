package org.opendaylight.opflex.genie.content.model.mtype;

import org.opendaylight.opflex.genie.engine.model.Cat;

/**
 * Created by midvorki on 7/28/14.
 */
public class MConstants
        extends SubTypeItem
{
    public static final Cat MY_CAT = Cat.getCreate("primitive:language:constants");
    public static final String NAME = "constants";
    public MConstants(
        MLanguageBinding aIn)
    {
        super(MY_CAT, aIn, NAME);
    }
}
