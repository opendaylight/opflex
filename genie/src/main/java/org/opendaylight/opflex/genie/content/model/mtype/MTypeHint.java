package org.opendaylight.opflex.genie.content.model.mtype;

import org.opendaylight.opflex.genie.engine.model.Cat;

/**
 * Created by dvorkinista on 7/7/14.
 */
public class MTypeHint
        extends SubTypeItem
{
    public static final Cat MY_CAT = Cat.getCreate("type:hint");
    public static final String NAME = "hint";

    public MTypeHint(
            MType aInType,
            TypeInfo aInTypeInfo)
    {
        super(MY_CAT, aInType, NAME);
        info = aInTypeInfo;
    }

    public TypeInfo getInfo()
    {
        return info;
    }

    private TypeInfo info;
}
