package org.opendaylight.opflex.genie.content.model.mvalidation;

import org.opendaylight.opflex.genie.engine.model.*;

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
        String aInTypeGNameOrNull)
    {
        super(MY_CAT,aInContentValidator,aInName);
        if (!(null == aInTypeGNameOrNull || aInTypeGNameOrNull.isEmpty()))
        {
            EXPLICIT_TYPE_REF.add(MY_CAT, getGID().getName(), MY_CAT, aInTypeGNameOrNull);
        }
    }
}
