package org.opendaylight.opflex.genie.content.model.mtype;

import org.opendaylight.opflex.genie.engine.model.Cardinality;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Relator;
import org.opendaylight.opflex.genie.engine.model.RelatorCat;
import org.opendaylight.opflex.modlan.report.Severity;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by dvorkinista on 7/7/14.
 */
public class MLanguageBinding
        extends SubTypeItem
{
    public static final Cat MY_CAT = Cat.getCreate("primitive:language-binding");
    public static final RelatorCat LIKE = RelatorCat.getCreate("primitive:language:constraints:use", Cardinality.SINGLE);

    /**
     * Constructor
     * @param aInType parent type for which this binding is defined
     * @param aInLang target language for which the binding is defined
     * @param aInSyntax  target language syntax
     * @param aInObjectSyntax target language object syntax
     * @param aInPassBy  identifies whether value is passed by: value, reference, pointer
     * @param aInPassConst identifies whether this type should always be passed as constant
     * @param aInInclude required include file for this type
     * @param aInLikePrimitiveGName language binding borrowing parameters from other language binding
     */
    public MLanguageBinding(
            MType aInType,
            Language aInLang,
            String aInSyntax,
            String aInObjectSyntax,
            PassBy aInPassBy,
            boolean aInPassConst,
            String aInInclude,
            String aInLikePrimitiveGName)
    {
        super(MY_CAT, aInType, aInLang.getName());
        if (aInType.isDerived())
        {
            Severity.DEATH.report(
                    this.toString(),
                    "add language binding",
                    "derived types don't have language bindings",
                    "can't add " + aInType + " language binding to type: " + aInType);
        }
        lang = aInLang;
        syntax = aInSyntax;

        objectSyntax = null == aInObjectSyntax ? syntax : aInObjectSyntax;
        passBy = null == aInPassBy ? PassBy.REFERENCE : aInPassBy;
        passConst = aInPassConst || (passBy == PassBy.REFERENCE);
        include = aInInclude;
        if (!Strings.isEmpty(aInLikePrimitiveGName))
        {
            LIKE.add(MY_CAT, getGID().getName(), MY_CAT, aInLikePrimitiveGName);
        }
    }

    /**
     * retrieves relator representing like relationship
     * @return relator that represents like relationship with another binding, null if doesn't exist
     */
    public Relator getLikeRelator()
    {
        return LIKE.getRelator(getGID().getName());
    }

    /**
     * checks if the binding has a like relationship with another binding
     * @return returns true if this binding has like, false otherwise
     */
    public boolean hasLike()
    {
        Relator lRel = LIKE.getRelator(getGID().getName());
        return null != lRel && lRel.hasTo();
    }

    /**
     * retrieves like language binding for this binding
     * @return like binding if like exists, null otherwise
     */
    public MLanguageBinding getLike()
    {
        Relator lRel = getLikeRelator();
        MLanguageBinding lLike =  (MLanguageBinding) (null == lRel ? null : lRel.getToItem());
        if (null == lLike)
        {
            MType lLikeType = getMType().getLiketype();
            if (null != lLikeType)
            {
                lLike = lLikeType.getLanguageBinding(lang);
            }
        }
        return lLike;
    }

    /**
     * Language accessor. Retrieves language with which this type binding is associated with
     * @return language with which this type binding is associated with
     */
    public Language getLanguage()
    {
        return lang;
    }

    /**
     * Syntax accessor. Retrieves target language syntax (like "uint32_t" in C++)
     * @return target language syntax
     */
    public String getSyntax()
    {
        if (Strings.isEmpty(syntax))
        {
            MLanguageBinding lLB = getLike();
            if (null != lLB)
            {
                syntax = lLB.getSyntax();
            }
        }
        if (Strings.isEmpty(syntax))
        {
            Severity.DEATH.report(this.toString(),"syntax retrieval","","no syntax defined");
        }
        return syntax;
    }

    /**
     * object syntax accessor: retrieves target language syntax for object containing this type...
     * for example, in java, a primitive int has object equivalent of java.lang.Integer
     * @return target language syntax for object containing this type
     */
    public String getObjectSyntax()
    {
        if (Strings.isEmpty(objectSyntax))
        {
            MLanguageBinding lLB = getLike();
            if (null != lLB)
            {
                objectSyntax = lLB.getObjectSyntax();
            }
        }
        if (Strings.isEmpty(objectSyntax))
        {
            Severity.WARN.report(this.toString(),"object syntax retrieval","","no object syntax defined: assuming same as syntax");
            objectSyntax = syntax;
        }
        return objectSyntax;
    }

    /**
     * identifies whether value is passed by: value, reference, pointer
     * @return type of parameter passing
     */
    public PassBy getPassBy()
    {
        if (PassBy.UNKNOWN == (passBy))
        {
            MLanguageBinding lLB = getLike();
            if (null != lLB)
            {
                passBy = lLB.getPassBy();
            }
        }
        if (PassBy.UNKNOWN == (passBy))
        {
            Severity.WARN.report(this.toString(),"pass by directive","","no pass by defined: assuming same as pass by reference");
            passBy = PassBy.REFERENCE;
        }
        return passBy;
    }

    /**
     * identifies if the value should be passed as constant
     * @return type should always be passed as constant (true by default)
     */
    public boolean getPassConst()
    {
        return passConst; // TODO:
    }

    /**
     * constraints accessor.
     * @return constrainst associated with this binding
     */
    public MConstraints getConstraints()
    {
        MConstraints lThat = null;
        for (MLanguageBinding lThis = this; null != lThis && null == lThat; lThis = lThis.getLike())
        {
            lThat = (MConstraints) lThis.getChildItem(MConstraints.MY_CAT,MConstraints.NAME);
        }
        return lThat;
    }

    public MConstants getConstants()
    {
        MConstants lThat = null;
        for (MLanguageBinding lThis = this; null != lThis && null == lThat; lThis = lThis.getLike())
        {
            lThat = (MConstants) lThis.getChildItem(MConstants.MY_CAT,MConstants.NAME);
        }
        return lThat;
    }

    public void validateCb()
    {
        super.validateCb();
        MConstraints lConstr = getConstraints();
        if (null == lConstr)
        {
            Severity.DEATH.report(toString(),"validation","","primitive types must have defined constraints");
        }
        MConstants lConsts = getConstants();
        if (null == lConsts)
        {
            Severity.DEATH.report(toString(),"validation","","primitive types must have defined constants");
        }
        MType lIndirType = lConstr.getType();
        if (null == lIndirType)
        {
            Severity.DEATH.report(toString(),"validation","","constraints type is unresolvable");
        }
    }

    /**
     * language with which this type binding is associated with
     */
    private final Language lang;

    /**
     * target language syntax for this type.
     # like "uint32_t"
     */
    private String syntax;

    /**
     * target language syntax for object containing this type...
     * for example, in java, a primitive int has object equivalent of java.lang.Integer
     */
    private String objectSyntax;

    /**
     * identifies whether value is passed by: value, reference, pointer
     */
    private PassBy passBy;

    /**
     * identifies whether this type should always be passed as constant (true by default)
     */
    private final boolean passConst;

    /**
     * required include file for this type
     */
    private final String include;
}