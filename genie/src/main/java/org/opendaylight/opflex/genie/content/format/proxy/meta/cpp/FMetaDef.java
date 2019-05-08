package org.opendaylight.opflex.genie.content.format.proxy.meta.cpp;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.mconst.ConstAction;
import org.opendaylight.opflex.genie.content.model.mconst.MConst;
import org.opendaylight.opflex.genie.content.model.mownership.MOwner;
import org.opendaylight.opflex.genie.content.model.mprop.MProp;
import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.content.model.mtype.MTypeHint;
import org.opendaylight.opflex.genie.content.model.mtype.TypeInfo;
import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.*;
import org.opendaylight.opflex.genie.engine.model.Ident;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.proc.Config;

import java.util.Collection;
import java.util.Map;
import java.util.TreeMap;

/**
 * Created by midvorki on 9/24/14.
 */
public class FMetaDef
        extends GenericFormatterTask
{
    public FMetaDef(
            FormatterCtx aInFormatterCtx,
            FileNameRule aInFileNameRule,
            Indenter aInIndenter,
            BlockFormatDirective aInHeaderFormatDirective,
            BlockFormatDirective aInCommentFormatDirective,
            boolean aInIsUserFile,
            WriteStats aInStats)
    {
        super(aInFormatterCtx,
              aInFileNameRule,
              aInIndenter,
              aInHeaderFormatDirective,
              aInCommentFormatDirective,
              aInIsUserFile,
              aInStats);
    }

    /**
     * Optional API required by the framework for transformation of file naming rule for the corresponding
     * generated file. This method can customize the location for the generated file.
     * @param aInFnr file name rule template
     * @return transformed file name rule
     */
    public static FileNameRule transformFileNameRule(FileNameRule aInFnr)
    {
        String lOldRelativePath = aInFnr.getRelativePath();
        String lNewRelativePath = lOldRelativePath + "/src/";

        return new FileNameRule(
                lNewRelativePath,
                null,
                aInFnr.getFilePrefix(),
                aInFnr.getFileSuffix(),
                aInFnr.getFileExtension(),
                "metadata");
    }

    public void generate()
    {
        out.println(0, "#include <" + Config.getProjName() + "/metadata/metadata.hpp>");
        out.println(0, "#include <" + Config.getProjName() + "/dmtree/Root.hpp>");

        out.println(0, "namespace " + Config.getProjName());
        out.println(0, "{");

        generateMetaAccessor(0);

        out.println(0, "} // namespace " + Config.getProjName());
    }

    private void generateMetaAccessor(int aInIndent)
    {
        out.println(aInIndent, "const opflex::modb::ModelMetadata& getMetadata()");
        out.println(aInIndent, "{");
        out.println(aInIndent + 1, "using namespace opflex::modb;");
        out.println(aInIndent + 1, "static const opflex::modb::ModelMetadata metadata(\"" +
                    Config.getProjName() + "\", ");
        generateClassDefs(aInIndent + 2);
        out.println(aInIndent + 2, ");");
        out.println(aInIndent + 1, "return metadata;");
        out.println(aInIndent, "}");
    }

    private void generateClassDefs(int aInIndent)
    {
        out.println(aInIndent, "{");
        boolean lFirst = true;
        for (Item lIt : MClass.getConcreteClasses())
        {
            MClass lClass = (MClass) lIt;
            if (lClass.isConcrete() && !isRelationshipTarget(lClass) && !isRelationshipResolver(lClass))
            {
                genMo(aInIndent + 2, lClass, lFirst);
                lFirst = false;
            }
        }
        out.println(aInIndent, "}");
    }

    private static String getClassType(MClass aIn)
    {
        if (isPolicy(aIn))
        {
            return "ClassInfo::POLICY";
        }
        else if (isObservable(aIn))
        {
            return "ClassInfo::OBSERVABLE";
        }
        else if (isRelationshipSource(aIn))
        {
            return "ClassInfo::RELATIONSHIP";
        }
        else if (isRelationshipTarget(aIn))
        {
            return "ClassInfo::REVERSE_RELATIONSHIP";
        }
        else  if (isRelationshipResolver(aIn))
        {
            return "ClassInfo::RESOLVER";
        }
        else if (!aIn.isConcrete())
        {
            return "ClassInfo::ABSTRACT";
        }
        else if (isGlobalEp(aIn))
        {
            return "ClassInfo::LOCAL_ENDPOINT";
        }
        else if (isLocalEp(aIn))
        {
            return "ClassInfo::LOCAL_ONLY";
        }
        else
        {
            return "ClassInfo::LOCAL_ONLY";
        }
    }
    
    private static String getTypeName(MType aIn)
    {
        String lType = aIn.getLID().getName().toUpperCase();
        switch (lType)
        {
            case "URI":
            case "IP":
            case "UUID":
                return "STRING";
            case "UINT64":
            case "UINT32":
            case "UINT16":
            case "UINT8":
            case "BITMASK8":
            case "BITMASK16":
            case "BITMASK32":
            case "BITMASK64":
                return "U64";
            default:
                return lType;
        }
    }

    private static boolean isLocalEp(MClass aIn)
    {
        return aIn.isSubclassOf("epr/LocalEp");

    }

    private static boolean isGlobalEp(MClass aIn)
    {
        return aIn.isSubclassOf("epr/ReportedEp");
    }

    private static boolean isPolicy(MClass aIn)
    {
        return aIn.isSubclassOf("policy/Component") || aIn.isSubclassOf("policy/Definition");
    }

    private static boolean isObservable(MClass aIn)
    {
        return aIn.isSubclassOf("observer/Observable");
    }

    private static boolean isRelationshipSource(MClass aIn)
    {
        return aIn.isConcreteSuperclassOf("relator/Source");
    }

    public static boolean isRelationshipTarget(MClass aIn)
    {
        return aIn.isConcreteSuperclassOf("relator/Target");
    }

    public static boolean isRelationshipResolver(MClass aIn)
    {
        return aIn.isConcreteSuperclassOf("relator/Resolver");
    }

    private static String getOwner(MClass aIn)
    {
        Collection<MOwner> lOwners = aIn.findOwners();
        return lOwners.isEmpty() ? (aIn.isConcrete() ? "default" : "abstract") : lOwners.iterator().next().getLID().getName();
    }

    private void genMo(int aInIndent, MClass aInClass, boolean aInIsFirst)
    {
        String lPrefix = aInIsFirst ? "" : ",";
        out.print(aInIndent , lPrefix + "ClassInfo(" + aInClass.getGID().getId() + ", ");
        out.println(getClassType(aInClass) + ", \"" + aInClass.getFullConcatenatedName() + "\", \"" + getOwner(aInClass) + "\",");
        genProps(aInIndent + 1, aInClass);
        out.println(aInIndent + 1, ')');
    }

    private String toUnsignedStr(int aInInt) {
    	return Long.toString(aInInt & 0xFFFFFFFFL) + "ul";
    }
    
    private void genProps(int aInIndent, MClass aInClass)
    {
        TreeMap<String,MProp> lProps = new TreeMap<>();
        aInClass.findProp(lProps,true);

        TreeMap<Ident,MClass> lConts = new TreeMap<>();
        aInClass.getContainsClasses(lConts, true, true);//false, true);

        if (lProps.size() + lConts.size() == 0)
        {
            out.println(aInIndent, "std::vector<prop_id_t>(),");
        }
        else
        {
            out.println(aInIndent, "{");
            boolean lIsFirst = true;
            // HANLDE PROPS
            for (MProp lProp : lProps.values())
            {
                MType lPropType = lProp.getType(false);
                MType lPrimitiveType = lPropType.getBuiltInType();
                MTypeHint lHint = lPrimitiveType.getTypeHint();

                int lLocalId = lProp.getPropId(aInClass);

                if (isRelationshipSource(aInClass)) {
                    if (lProp.getLID().getName().equalsIgnoreCase("targetClass"))
                        continue;
                    else if (lProp.getLID().getName().equalsIgnoreCase("targetName")) {
                        out.println(aInIndent + 1,
                                    (lIsFirst ?  "" : ",") + "(PropertyInfo(" + toUnsignedStr(lLocalId) + ", \"target\", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // " + lProp.toString());
                        lIsFirst = false;
                    }
                }
                else if (lProp.getLID().getName().equalsIgnoreCase("source") && isRelationshipTarget(aInClass))
                {
                    out.println(aInIndent + 1,
                                (lIsFirst ?  "" : ",") + "(PropertyInfo(" + toUnsignedStr(lLocalId) + ", \"source\", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // " + lProp.toString());
                    lIsFirst = false;
                }
                else if (Config.isEnumSupport() &&
                         (lHint.getInfo() == TypeInfo.ENUM ||
                          lHint.getInfo() == TypeInfo.BITMASK))
                {
                    out.println(
                        aInIndent + 1,
                        (lIsFirst ?  "" : ",") + "(PropertyInfo(" + toUnsignedStr(lLocalId) + ", \"" + lProp.getLID().getName() + "\", PropertyInfo::" + getTypeName(lPrimitiveType) + ", PropertyInfo::SCALAR,");
                    genConsts(aInIndent + 2, aInClass, lProp, lPropType);
                    out.println(")) // " + lProp.toString());
                    lIsFirst = false;
                }
                else
                {
                    out.println(
                        aInIndent + 1,
                        (lIsFirst ?  "" : ",") + "(PropertyInfo(" + toUnsignedStr(lLocalId) + ", \"" + lProp.getLID().getName() + "\", PropertyInfo::" + getTypeName(lPrimitiveType) + ", PropertyInfo::SCALAR)) // "
                        + lProp.toString());
                    lIsFirst = false;
                }
            }


            // HANDLE CONTAINED CLASSES
            for (MClass lContained : lConts.values())
            {
                if (!isRelationshipTarget(lContained) && !isRelationshipResolver(lContained))
                {
                    out.println(aInIndent + 1, (lIsFirst ? "" : ",") + "(PropertyInfo(" + toUnsignedStr(lContained.getClassAsPropId(aInClass)) + ", \"" + lContained.getFullConcatenatedName() + "\", PropertyInfo::COMPOSITE, " + lContained.getGID().getId() + ", PropertyInfo::VECTOR)) // " + lContained.toString());
                    lIsFirst = false;
                }
            }

            out.println(aInIndent + 1, "}");
        }
    }
    private void genConsts(int aInIndent, MClass aInClass, MProp aInProp, MType aInType)
    {
        Map<String, MConst> lConsts = new TreeMap<>();
        aInProp.findConst(lConsts, true);
        out.println(aInIndent, "EnumInfo(\"" + aInType.getFullConcatenatedName() + "\",");

        if (lConsts.isEmpty())
        {
            out.println(aInIndent, "std::vector<ConstInfo>()");
        }
        else
        {
            int lCount = 0;
            for (MConst lConst : lConsts.values())
            {
                if (ConstAction.REMOVE != lConst.getAction())
                {
                    lCount++;
                    if (1 == lCount)
                    {
                        out.println(aInIndent + 1, "{");
                    }
                    out.println(aInIndent + 3, (lCount > 1 ? "," : "") + "(ConstInfo(\"" + lConst.getLID().getName() + "\", " + lConst.getValue().getValue() + ")) // " + lConst);
                }
            }
            if (0 < lCount)
            {
                out.println(aInIndent + 1, "}");
            }
            if (0 == lCount)
            {
                out.println(aInIndent, "std::vector<ConstInfo>() // NO CONSTS DEFINED");
            }
        }
        out.print(aInIndent + 1,")");
    }
}
