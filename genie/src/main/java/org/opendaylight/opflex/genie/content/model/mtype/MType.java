package org.opendaylight.opflex.genie.content.model.mtype;

import java.util.*;

import org.opendaylight.opflex.genie.content.model.mconst.MConst;
import org.opendaylight.opflex.genie.content.model.module.Module;
import org.opendaylight.opflex.genie.content.model.module.SubModuleItem;
import org.opendaylight.opflex.genie.content.model.mvalidation.MValidator;
import org.opendaylight.opflex.genie.engine.model.*;
import org.opendaylight.opflex.modlan.report.Severity;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by dvorkinista on 7/7/14.
 */
public class MType extends SubModuleItem
{
    public static final Cat MY_CAT = Cat.getCreate("mtype");
    public static final RelatorCat SUPER_CAT = RelatorCat.getCreate("type:super", Cardinality.SINGLE);
    public static final RelatorCat LIKE_CAT = RelatorCat.getCreate("primitive:like", Cardinality.SINGLE);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CONSTRUCTION
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Constructor for non-built-in/non-base types.
     *
     * @param aInModule name of the module to which this type belongs
     * @param aInLName name of the type
     */
    public MType(
            Module aInModule,
            String aInLName)
    {
        this(aInModule,aInLName,true);
    }

    /**
     * general constructor
     * @param aInModule name of the module to which this type belongs
     * @param aInLName name of the type
     * @param aInIsBuiltIn specifies whether this type is built-in
     */
    public MType(
            Module aInModule,
            String aInLName,
            boolean aInIsBuiltIn
            )
    {
        super(MY_CAT, aInModule, aInLName);
        isBuiltIn = aInIsBuiltIn;
    }

    public String getFullConcatenatedName()
    {
        return Strings.upFirstLetter(getModule().getLID().getName()) + Strings.upFirstLetter(getLID().getName()) + "T";
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // TYPE RETRIEVAL APIs
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Instance accessor: finds type by name
     * @param aInGName name of the type retrieved
     * @return type corresponding to the name.
     */
    public static MType get(String aInGName)
    {
        return (MType) MY_CAT.getItem(aInGName);
    }

    /**
     * retrieves containing type for a sub-type item
     * @param aIn sub-type item like language binding, constant, range constraint etc.
     * @return containing type
     */
    public static MType getMType(Item aIn)
    {
        return (MType) aIn.getAncestorOfCat(MType.MY_CAT);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // BUILT-IN/DERIVED APIs
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Determines if this type represents a built-in type that has all of the language binding and hints specifications.
     * Basically, the opposite of derived type.
     * @return true if this is a built in (or non-derived) type; false otherwise.
     */
    public boolean isBuiltIn()
    {
        return isBuiltIn;
    }

    /**
     * Determines whether this type represents a derived type that is inheriting/sub-typed from some other type.
     * Basically, the opposite of built-in type.
     * @return true if this is a derived (or non-built-in) type; false otherwise
     */
    public boolean isDerived()
    {
        return !isBuiltIn();
    }

    /**
     * Determines if this type represents a base (built-in) type that has all of the language binding and hints specifications.
     * Basically, the opposite of derived type. Synonymous to isBuiltIn().
     * @return true if this is a built in (or non-derived) type; false otherwise.
     */
    public boolean isBase()
    {
        return isBuiltIn();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // DERIVED TYPE APIs: SUB-TYPES/SUPER-TYPES
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * registers super type for this type. super type is the type from which this type is derived
     * @param aInTargetGName super type global name
     */
    public void addSupertype(String aInTargetGName)
    {
        if (!Strings.isEmpty(aInTargetGName))
        {
            if (isBuiltIn())
            {
                Severity.DEATH.report(this.toString(), "add super-type", "built-in/base type can't have super-types",
                                      "can't derive from " + aInTargetGName);
            }
            else if (getGID().getName().equals(aInTargetGName))
            {
                Severity.DEATH
                        .report(this.toString(), "add super-type", "can't reference self", "can't derive from self");
            }
            SUPER_CAT.add(MY_CAT, getGID().getName(), MY_CAT, aInTargetGName);
        }
    }

    /**
     * retrieves relator representing supertype relationship
     * @return relator that represents supertype relationship with another type, null if doesn't exist
     */
    public Relator getSupertypeRelator()
    {
        return SUPER_CAT.getRelator(getGID().getName());
    }

    /**
     * checks if the type has a supertype
     * @return returns true if this type has supertype, false otherwise
     */
    public boolean hasSupertype()
    {
        Relator lRel = SUPER_CAT.getRelator(getGID().getName());
        return null != lRel && lRel.hasTo();
    }

    /**
     * retrieves supertpype of this class
     * @return supertype if supertype exists, null otherwise
     */
    public MType getSupertype()
    {
        Relator lRel = getSupertypeRelator();
        return (MType) (null == lRel ? null : lRel.getToItem());
    }

    /**
     * retrieves all supertypes for this type. supertypes are added in order of distance from the subtype.
     * @param aOut collection of supertypes
     * @param aInIncludeSelf identifies whether to include this type
     */
    public void getSupertypes(Collection<MType> aOut, boolean aInIncludeSelf)
    {
        for (MType lThat = aInIncludeSelf ? this : getSupertype();
             null != lThat;
             lThat = lThat.getSupertype())
        {
            aOut.add(lThat);
        }
    }

    /**
     * accesses built-in/non-derived/base type. synonymous with getBuiltInType()
     * @return built-in/base type
     */
    public MType getBase()
    {
        return getBuiltInType();
    }

    /**
     * retrieves built-in super-type accessor.
     * @return built-in/base type
     */
    public MType getBuiltInType()
    {
        if (isBuiltIn())
        {
            return this;
        }
        else
        {
            for (MType lThat = getSupertype(); null != lThat; lThat = lThat.getSupertype())
            {
                if (lThat.isBuiltIn())
                {
                    return lThat;
                }
            }
            Severity.DEATH.report(
                    this.toString(),
                    "built in supertype retrieval",
                    "no built in type",
                    "can't find built in type");

            return null;
        }
    }

    /**
     * checks if this type has subtypes
     * @return true if this type has subtypes. false otherwise.
     */
    public boolean hasSubtypes()
    {
        Relator lInvRel = SUPER_CAT.getInverseRelator(getGID().getName());
        return null != lInvRel && lInvRel.hasTo();
    }

    /**
     * Retrieves subclasses of this class
     * @param aOut collection of subtypes of this type
     * @param aInIsDirectOnly specifies if only direct subtypes are returned
     */
    public void getSubtypes(
            Collection<MType> aOut,
            boolean aInIsDirectOnly)
    {
        Relator lInvRel = SUPER_CAT.getInverseRelator(getGID().getName());
        if (null != lInvRel)
        {
            for (Item lItem : lInvRel.getToItems())
            {
                MType lType = (MType) lItem;
                aOut.add(lType);
                if (!aInIsDirectOnly)
                {
                    lType.getSubtypes(aOut, aInIsDirectOnly);
                }
            }
        }
    }

    /**
     * Retrieves subtypes of this type in type inheritance tree
     * @param aOut collection of subtypes of this type
     */
    public void getSubtypes(Collection<MType> aOut)
    {
        getSubtypes(aOut, false);
    }

    /**
     * Retrieves subtypes of this type in type inheritance tree
     * @return collection of subtypes of this type
     */
    public Collection<MType> getSubtypes()
    {
        LinkedList<MType> lRet = new LinkedList<MType>();
        getSubtypes(lRet, false);
        return lRet;
    }

    /**
     * Retrieves direct subtypes of this type in type inheritance tree
     * @param aOut collection of direct subtypes of this type
     */
    public void getDirectSubtypes(Collection<MType> aOut)
    {
        getSubtypes(aOut, true);
    }

    /**
     * Retrieves direct subtypes of this type in type inheritance tree
     * @return collection of direct subtypes of this type
     */
    public Collection<MType> getDirectSubtypes()
    {
        LinkedList<MType> lRet = new LinkedList<MType>();
        getSubtypes(lRet, true);
        return lRet;
    }

    
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // PRIMITIVE LIKE API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    public void addLiketype(String aInTargetGName)
    {
        if (!Strings.isEmpty(aInTargetGName))
        {
            if (!isBuiltIn())
            {
                Severity.DEATH.report(this.toString(), "add like-type", "built-in/base type can't have like-types",
                                      "can't derive from " + aInTargetGName);
            }
            else if (getGID().getName().equals(aInTargetGName))
            {
                Severity.DEATH
                        .report(this.toString(), "add like-type", "can't reference self", "can't derive from self");
            }
            LIKE_CAT.add(MY_CAT, getGID().getName(), MY_CAT, aInTargetGName);
        }
    }

    /**
     * retrieves relator representing liketype relationship
     * @return relator that represents liketype relationship with another type, null if doesn't exist
     */
    public Relator getLiketypeRelator()
    {
        return LIKE_CAT.getRelator(getGID().getName());
    }

    /**
     * checks if the type has a liketype
     * @return returns true if this type has liketype, false otherwise
     */
    public boolean hasLiketype()
    {
        Relator lRel = LIKE_CAT.getRelator(getGID().getName());
        return null != lRel && lRel.hasTo();
    }

    /**
     * retrieves liketpype of this class
     * @return liketype if liketype exists, null otherwise
     */
    public MType getLiketype()
    {
        Relator lRel = getLiketypeRelator();
        return (MType) (null == lRel ? null : lRel.getToItem());
    }

    /**
     * retrieves all liketypes for this type. liketypes are added in order of distance from the subtype.
     * @param aOut collection of liketypes
     * @param aInIncludeSelf identifies whether to include this type
     */
    public void getLiketypes(Collection<MType> aOut, boolean aInIncludeSelf)
    {
        for (MType lThat = aInIncludeSelf ? this : getLiketype();
             null != lThat;
             lThat = lThat.getLiketype())
        {
            aOut.add(lThat);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // BUILT-IN LANGUAGE BINDING AND HINTs APIs
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * retrieves language binding descriptor for this specific type
     * @param aIn language for which binding is to be retrieved
     * @return language binding for this type given the specified language (aIn)
     */
    public MLanguageBinding getLanguageBinding(Language aIn)
    {
        MLanguageBinding lLb = null;
        for (MType lThis = getBuiltInType(); null != lThis && null == lLb; lThis = lThis.getLiketype())
        {
            lLb = (MLanguageBinding) lThis.getChildItem(MLanguageBinding.MY_CAT, aIn.getName());
        }

        if (null == lLb)
        {
            Severity.DEATH.report(
                    this.toString(),
                    "language binding retrieval",
                    "language binding not found",
                    "no language binding defined for " +
                    aIn +
                    ": has like type: " + hasLiketype() + " : " + getLiketype());
        }

        return lLb;
    }

    /**
     * retrieves type hint for this specific type
     * @return type hint for this type
     */
    public MTypeHint getTypeHint()
    {
        MTypeHint lTh = null;
        for (MType lThis = getBuiltInType(); null != lThis && null == lTh; lThis = lThis.getLiketype())
        {
            lTh = (MTypeHint) lThis.getChildItem(MTypeHint.MY_CAT, MTypeHint.NAME);
        }
        if (null == lTh)
        {
            Severity.DEATH.report(
                    this.toString(),
                    "type hint retrieval",
                    "type hint not found",
                    "no type hint defined for");
        }

        return lTh;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CONSTANT APIs
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * retrieves constant defined under this type by name
     * @param aInName name of the constant to be retrieved.
     * @return Constant associated with the name passed in that is defined under this type
     */
    public MConst getConst(String aInName)
    {
        return (MConst) getChildItem(MConst.MY_CAT, aInName);
    }

    /**
     * finds constant defined under this type or, if specified, any of the supertypes, by name
     * @param aInName name of the constant to be retrieved.
     * @param aInCheckSuperTypes identifies that supertypes are to be checked
     * @return
     */
    public MConst findConst(String aInName, boolean aInCheckSuperTypes)
    {
        MConst lConst = null;
        for (MType lThisType = this;
             null != lThisType && null == lConst;
             lThisType = aInCheckSuperTypes ? lThisType.getSupertype() : null)
        {
            lConst = lThisType.getConst(aInName);

            if (null != lConst && lThisType.isBase())
            {
                for (lThisType = lThisType.getLiketype();
                     null != lThisType && null == lConst;
                     lThisType = lThisType.getLiketype())
                {
                    lConst = lThisType.getConst(aInName);
                }
            }
        }

        return lConst;
    }
    /**
     * retrieves all constants defined under this type
     * @param aOut  All constants defined under this type
     */
    public void getConst(Map<String, MConst> aOut)
    {
        Collection<Item> lItems = new LinkedList<Item>();
        getChildItems(MConst.MY_CAT,lItems);

        if (isBase())
        {
            for (MType lThisType = this.getLiketype();
                 null != lThisType;
                 lThisType = lThisType.getLiketype())
            {
                lThisType.getChildItems(MConst.MY_CAT,lItems);

            }
        }
        for (Item lItem : lItems)
        {
            if (!aOut.containsKey(lItem.getLID().getName()))
            {
                aOut.put(lItem.getLID().getName(), (MConst) lItem);
            }
        }
    }


    /**
     * retrieves all constants defined under this type or, if specified, any of the supertypes
     * @param aOut  All constants defined under this type or, if specified, any of the supertypes
     * @param aInCheckSuperTypes identifies that supertypes are to be checked
     */
    public void findConst(Map<String, MConst> aOut, boolean aInCheckSuperTypes)
    {
        for (MType lThisType = this;
             null != lThisType;
             lThisType = aInCheckSuperTypes ? lThisType.getSupertype() : null)
        {
            lThisType.getConst(aOut);
        }
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // VALIDATOR APIs
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * retrieves validator defined under this type by name
     * @param aInName name of the validator to be retrieved.
     * @return Validator associated with the name passed in that is defined under this type
     */
    public MValidator getValidator(String aInName)
    {
        return (MValidator) getChildItem(MValidator.MY_CAT, aInName);
    }

    /**
     * finds validator defined under this type or, if specified, any of the supertypes, by name
     * @param aInName name of the validator to be retrieved.
     * @param aInCheckSuperTypes identifies that supertypes are to be checked
     * @return
     */
    public MValidator findValidator(String aInName, boolean aInCheckSuperTypes)
    {
        MValidator lValidator = null;
        for (MType lThisType = this;
             null != lThisType &&
             null == lValidator;
             lThisType = aInCheckSuperTypes ? lThisType.getSupertype() : null)
        {
            lValidator = lThisType.getValidator(aInName);
            if (null != lValidator && lThisType.isBase())
            {
                for (lThisType = lThisType.getLiketype();
                     null != lThisType && null == lValidator;
                     lThisType = lThisType.getLiketype())
                {
                    lValidator = lThisType.getValidator(aInName);
                }
            }
        }
        return lValidator;
    }
    /**
     * retrieves all validators defined under this type
     * @param aOut  All validators defined under this type
     */
    public void getValidator(Map<String, MValidator> aOut)
    {
        Collection<Item> lItems = new LinkedList<Item>();
        getChildItems(MValidator.MY_CAT,lItems);
        if (isBase())
        {
            for (MType lThisType = this.getLiketype();
                 null != lThisType;
                 lThisType = lThisType.getLiketype())
            {
                lThisType.getChildItems(MValidator.MY_CAT,lItems);

            }
        }
        for (Item lItem : lItems)
        {
            if (!aOut.containsKey(lItem.getLID().getName()))
            {
                aOut.put(lItem.getLID().getName(), (MValidator) lItem);
            }
        }
    }

    /**
     * retrieves all validators defined under this type or, if specified, any of the supertypes
     * @param aOut  All validators defined under this type or, if specified, any of the supertypes
     * @param aInCheckSuperTypes identifies that supertypes are to be checked
     */
    public void findValidator(Map<String, MValidator> aOut, boolean aInCheckSuperTypes)
    {
        for (MType lThisType = this;
             null != lThisType;
             lThisType = aInCheckSuperTypes ? lThisType.getSupertype() : null)
        {
            lThisType.getValidator(aOut);
        }
    }

    public void validateCb()
    {
        super.validateCb();
        getBase();
        findConst(new TreeMap<String, MConst>(), true);
        findValidator(new TreeMap<String, MValidator>(), true);
        getTypeHint();
        for (Language lLang : Language.values())
        {
            getLanguageBinding(lLang);
        }
    }
    private final boolean isBuiltIn;
}
