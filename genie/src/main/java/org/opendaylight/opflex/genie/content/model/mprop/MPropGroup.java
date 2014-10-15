package org.opendaylight.opflex.genie.content.model.mprop;

import java.util.Map;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.mclass.SubStructItem;
import org.opendaylight.opflex.genie.engine.model.Cat;

/**
 * Created by midvorki on 8/16/14.
 */
public class MPropGroup extends SubStructItem
{
    public static final Cat MY_CAT = Cat.getCreate("mprop:group");

    public MPropGroup(MClass aInClass, String aInName)
    {
        super(MY_CAT,aInClass,aInName);
    }

    public MClass getMClass()
    {
        return (MClass) getParent();
    }

    public void getProps(Map<String,MProp> aOutProps)
    {
        getProps(getMClass(),aOutProps);
    }

    public void getProps(MClass aInClass, Map<String,MProp> aOutProps)
    {
        aInClass.findProp(aOutProps, getLID().getName());
    }
}
