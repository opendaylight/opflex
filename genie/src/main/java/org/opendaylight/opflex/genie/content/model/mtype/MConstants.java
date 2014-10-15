package org.opendaylight.opflex.genie.content.model.mtype;

import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.modlan.report.Severity;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/28/14.
 */
public class MConstants
        extends SubTypeItem
{
    public static final Cat MY_CAT = Cat.getCreate("primitive:language:constants");
    public static final String NAME = "constants";
    public MConstants(
            MLanguageBinding aIn,
            DefinedIn aInDefinedIn,
            String aInPrefix,
            String aInSuffix)
    {
        super(MY_CAT, aIn, NAME);
        definedIn = null == aInDefinedIn ? DefinedIn.SPECIAL : aInDefinedIn;
        prefix = aInPrefix;
        suffix = aInSuffix;
    }

    public MConstants getLike()
    {
        MConstants lThis = null;
        for (MLanguageBinding lLB = getLanguageBinding().getLike(); null != lLB && null == lThis; lLB = lLB.getLike())
        {
            lThis = (MConstants) lLB.getChildItem(MConstants.MY_CAT,MConstraints.NAME);
        }
        return lThis;
    }

    /**
     * retrieves what format values are to be defined.
     * for example: dec, hex, octal, ...
     * @return format in which constant values are defined in
     */
    public DefinedIn getDefinedIn()
    {
        if (DefinedIn.UNKNOWN == (definedIn))
        {
            MConstants lCs = getLike();
            if (null != lCs)
            {
                definedIn = lCs.getDefinedIn();
            }
        }
        if (DefinedIn.UNKNOWN == (definedIn))
        {
            Severity.WARN.report(this.toString(),"defined-in-format","","no rule defined: assuming special");
            definedIn = DefinedIn.SPECIAL;
        }
        return definedIn;
    }
    /**
     * retrieves a literal prefix that is required before constant definition.
     * as in 0x.... or something like that
     * @return constant's literal prefix
     */
    public String getPrefix()
    {
        if (Strings.isEmpty(prefix))
        {
            MConstants lCs = getLike();
            if (null != lCs)
            {
                prefix = lCs.getPrefix();
            }
        }
        return prefix;
    }

    /**
     * @return whether the corresponding constants have literal prefix
     */
    public boolean hasPrefix() { return !Strings.isEmpty(getPrefix()); }

    /**
     * retrieves literal suffix that is required before constant definition.
     * as in ll, ull: ... = ...6666ull
     * @return constant's literal suffix
     */
    public String getSuffix()
    {
        if (Strings.isEmpty(suffix))
        {
            MConstants lCs = getLike();
            if (null != lCs)
            {
                suffix = lCs.getSuffix();
            }
        }
        return suffix;
    }

    /**
     * @return whether the corresponding constants have literal suffix
     */
    public boolean hasSuffix() { return !Strings.isEmpty(getSuffix()); }

    public MLanguageBinding getLanguageBinding()
    {
        return (MLanguageBinding) getParent();
    }

    /**
     * returns datatype that is used for these constants
     * @return datatype used for these constants
     */
    public MType getType()
    {
        MConstraints lConstrs = getLanguageBinding().getConstraints();
        if (null != lConstrs)
        {
            return getType();
        }
        else
        {
            return super.getMType();
        }
    }

    /**
     * specifies what format values are to be defined.
     * for example: dec, hex, octal, ...
     */
    private DefinedIn definedIn;
    /**
     * a literal prefix that is required before constant definition.
     * as in 0x.... or something like that
     */
    private String prefix;
    /**
     * literal suffix that is required before constant definition.
     * as in ll, ull: ... = ...6666ull
     */
    private String suffix;
}
