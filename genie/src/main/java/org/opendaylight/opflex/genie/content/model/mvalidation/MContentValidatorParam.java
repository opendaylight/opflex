package org.opendaylight.opflex.genie.content.model.mvalidation;

import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.engine.model.*;
import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/10/14.
 */
public class MContentValidatorParam
        extends Item
{
    public static final Cat MY_CAT = Cat.getCreate("mvalidator:mcontent:param");
    public static final RelatorCat EXPLICIT_TYPE_REF = RelatorCat.getCreate(
            "mvalidator:mcontent:param:explicit-type-ref", Cardinality.SINGLE);

    public MContentValidatorParam(
            MContentValidator aInContentValidator,
            String aInName,
            String aInValue,
            String aInTypeGNameOrNull)
    {
        super(MY_CAT,aInContentValidator,aInName);
        value = aInValue;
        if (!(isValueInheritedFromContainer =
                    (null == aInTypeGNameOrNull || aInTypeGNameOrNull.isEmpty())))
        {
            EXPLICIT_TYPE_REF.add(MY_CAT, getGID().getName(), MY_CAT, aInTypeGNameOrNull);
        }
    }

    public MValidator getValidator()
    {
        return getContentValidator().getValidator();
    }

    public MContentValidator getContentValidator()
    {
        return (MContentValidator) getParent();
    }

    public MType getType(boolean aInIsBaseType)
    {
        MType lType = null;
        if (isValueInheritedFromContainer)
        {
            lType = getContentValidator().getType(aInIsBaseType);
        }
        else
        {
            Relator lRel = EXPLICIT_TYPE_REF.getRelator(getGID().getName());
            if (null != lRel)
            {
                lType = (MType) lRel.getToItem();
                lType = aInIsBaseType ? lType.getBase() : lType;
            }
            else
            {
                Severity.DEATH.report(
                        this.toString(),
                        "content validator param type retrieval",
                        "no associated type found",
                        "explicit type refernce not found");
            }
        }
        return lType;
    }

    // HAS VALUE AND TYPE
    private final String value;
    private final boolean isValueInheritedFromContainer;
}
