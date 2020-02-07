package org.opendaylight.opflex.genie.content.model.mtype;

import org.opendaylight.opflex.genie.engine.model.Cardinality;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Relator;
import org.opendaylight.opflex.genie.engine.model.RelatorCat;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/28/14.
 */
public class MConstraints
        extends SubTypeItem
{
    public static final Cat MY_CAT = Cat.getCreate("primitive:language:constraints");
    public static final RelatorCat USE = RelatorCat.getCreate("primitive:language:constraints:use", Cardinality.SINGLE);

    public static final String NAME = "constraints";

    /**
     * Constructor
     * @param aInBind containing language binding
     * @param aInUseTypeGName type indirection specification: use another type for constraints if not null/empty
     */
    public MConstraints(
        MLanguageBinding aInBind,
        String aInUseTypeGName)

    {
        super(MY_CAT, aInBind, NAME);
        if (usesOtherType = !Strings.isEmpty(aInUseTypeGName))
        {
            USE.add(MY_CAT, getGID().getName(), MType.MY_CAT, aInUseTypeGName);
        }
    }

    public MLanguageBinding getLanguageBinding()
    {
        return (MLanguageBinding) getParent();
    }

    public MConstraints getLike()
    {
        MConstraints lThis = null;
        for (MLanguageBinding lLB = getLanguageBinding().getLike(); null != lLB && null == lThis; lLB = lLB.getLike())
        {
            lThis = (MConstraints) lLB.getChildItem(MConstraints.MY_CAT,MConstraints.NAME);
        }
        return lThis;
    }

    /**
     * Specifies if the constraints use some other datatype. Target type that allows constraints to be
     * defined in another type (useful for strings etc.) If range specification requires a different type
     * than the one that the range is contained by, this construct allows type indirection, where another type is
     * used for definition of the range
     * @return whether this type relies on other type for constraints
     */
    public boolean isUsesOtherType() { return usesOtherType; }

    public Relator getUseRelator()
    {
        return USE.getRelator(getGID().getName());
    }

    /**
     * returns datatype that is used for these constraints
     * @return datatype used for these constraints
     */
    public MType getType()
    {
        for (MConstraints lThis = this; null != lThis; lThis = lThis.getLike())
        {
            if (lThis.isUsesOtherType())
            {
                Relator lRel = getUseRelator();
                return (MType) (null == lRel ? super.getMType() : lRel.getToItem());
            }
        }
        return super.getMType();
    }

    /**
     * Specifies if the constraints use some other datatype. Target type that allows constraints to be
     * defined in another type (useful for strings etc.) If range specification requires a different type
     * than the one that the range is contained by, this construct allows type indirection, where another type is
     * used for definition of the range
     */
    private final boolean usesOtherType;
}
