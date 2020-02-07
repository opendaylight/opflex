package org.opendaylight.opflex.genie.content.model.mnaming;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.mprop.MProp;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.modlan.report.Severity;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 9/27/14.
 *
 * Name component is a component of specific name rule that is comprised of two prefix and naming property:
 * &lt;prefix&gt;-&lt;/property-name&gt;
 * Both prefix and naming prperty are optional. Prefix can be used in LN/RN or LRI formation
 */
public class MNameComponent extends Item
{
    /**
     * category identifying all containment specific naming component
     */
    public static final Cat MY_CAT = Cat.getCreate("mnamer:component");

    /**
     * Constructor
     * @param aInRule naming rule under which this component is instantiated
     * @param aInLocalPropertyNameOrNull optional component naming property name
     */
    public MNameComponent(MNameRule aInRule, String aInLocalPropertyNameOrNull)
    {
        super(MY_CAT, aInRule, aInRule.getNextComponentName());
        propName = aInLocalPropertyNameOrNull;
    }

    /**
     * naming property name accessor
     * @return ame of the naming property if such specified
     */
    public String getPropName()
    {
        return propName;
    }

    /**
     * checks if naming prperty name is specified
     * @return true in naming property name is specified
     */
    public boolean hasPropName()
    {
        return !Strings.isEmpty(propName);
    }

    /**
     * containing name rule accessor
     * @return containing name rule
     */
    public MNameRule getRule()
    {
        return (MNameRule) getParent();
    }

    /**
     * containing namer accessor
     * @return containing namer
     */
    public MNamer getNamer()
    {
        return getRule().getNamer();
    }

    /**
     * traget class accessor
     * @return retrieves the class for which this naming rule is specified
     */
    public MClass getTargetClass()
    {
        return getNamer().getTargetClass();
    }

    /**
     * naming property accessor
     * @return naming property corresponding to this component if such is specified, null otherwise
     */
    public MProp getProp()
    {
        return hasPropName() ? getTargetClass().findProp(propName,false) : null;
    }

    public void validateCb()
    {
        super.validateCb();
        if (hasPropName())
        {
            MProp lProp = getProp();
            if (null == lProp)
            {
                Severity.DEATH.report(
                        "validation","naming rule validation", "naming prop not found",
                        "no such prop " + propName + " in class " + getNamer().getLID().getName());
            }
        }
    }

    private final String propName;
}
