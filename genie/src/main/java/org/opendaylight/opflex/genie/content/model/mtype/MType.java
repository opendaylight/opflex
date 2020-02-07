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
     * retrieves supertpype of this class
     * @return supertype if supertype exists, null otherwise
     */
    public MType getSupertype()
    {
        Relator lRel = getSupertypeRelator();
        return (MType) (null == lRel ? null : lRel.getToItem());
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
     * retrieves all constants defined under this type
     * @param aOut  All constants defined under this type
     */
    public void getConst(Map<String, MConst> aOut)
    {
        Collection<Item> lItems = new LinkedList<>();
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


    public Collection<MConst> getConst()
    {
        Map<String,MConst> lConsts = new TreeMap<>();
        getConst(lConsts);
        return lConsts.values();
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

    public Item getClosestConstantHolder()
    {
        for (MType lThisType = this;
             null != lThisType;
             lThisType = lThisType.getSupertype())
        {
            if (lThisType.hasChildren(MConst.MY_CAT))
            {
                return lThisType;
            }
        }
        return null;
    }

    public Item getNextConstantHolder()
    {
        for (MType lThisType = this;
             null != lThisType;
             lThisType = lThisType.getSupertype())
        {
            if (this != lThisType && lThisType.hasConstants())
            {
                return lThisType;
            }
        }
        return null;
    }

    public boolean hasConstants()
    {
        return hasChildren(MConst.MY_CAT);
    }

    public boolean hasEnumeratedConstants()
    {
        return getBuiltInType().getTypeHint().getInfo().isEnumerated() && hasConstants();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // VALIDATOR APIs
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * retrieves all validators defined under this type
     * @param aOut  All validators defined under this type
     */
    public void getValidator(Map<String, MValidator> aOut)
    {
        Collection<Item> lItems = new LinkedList<>();
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
        findConst(new TreeMap<>(), true);
        findValidator(new TreeMap<>(), true);
        getTypeHint();
        for (Language lLang : Language.values())
        {
            getLanguageBinding(lLang);
        }
    }
    private final boolean isBuiltIn;
}
