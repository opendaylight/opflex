package org.opendaylight.opflex.genie.content.format.meta.cpp;

import java.util.Collection;
import java.util.TreeMap;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.mnaming.MNameComponent;
import org.opendaylight.opflex.genie.content.model.mnaming.MNameRule;
import org.opendaylight.opflex.genie.content.model.mownership.MOwner;
import org.opendaylight.opflex.genie.content.model.mprop.MProp;
import org.opendaylight.opflex.genie.content.model.mrelator.MRelationshipClass;
import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.*;
import org.opendaylight.opflex.genie.engine.model.Ident;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.proc.Config;

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

        FileNameRule lFnr = new FileNameRule(
                lNewRelativePath,
                null,
                aInFnr.getFilePrefix(),
                aInFnr.getFileSuffix(),
                aInFnr.getFileExtension(),
                "metadata");

        return lFnr;
    }

    public void generate()
    {
        out.println(0, "#include <boost/assign/list_of.hpp>");
        out.println(0, "#include \"" + Config.getProjName() + "/metadata/metadata.hpp\"");
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
        out.println(aInIndent + 1, "using namespace boost::assign;");
            out.println(aInIndent + 1, "static const opflex::modb::ModelMetadata metadata(\"" + 
                    Config.getProjName() + "\", ");
            generateClassDefs(aInIndent + 2);
            out.println(aInIndent + 2, ");");
            out.println(aInIndent + 1, "return metadata;");
        out.println(aInIndent, "}");
    }

    private void generateClassDefs(int aInIndent)
    {
        out.println(aInIndent, "list_of");
        for (Item lIt : MClass.getConcreteClasses())
        {
            MClass lClass = (MClass) lIt;
            if (lClass.isConcrete())
            {
                genMo(aInIndent + 2, lClass);
            }
        }
    }

    public static String getClassType(MClass aIn)
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
            return "ClassInfo::RELATIONSHIP";
        }
        else  if (isRelationshipResolver(aIn))
        {
            return "ClassInfo::RESOLVER";
        }
        else if (!aIn.isConcrete())
        {
            return "ClassInfo::ABSTRACT";
        }
        else
        {
            return "ClassInfo::LOCAL_ONLY";
        }
    }
    
    public static String getTypeName(MType aIn)
    {
        String lType = aIn.getLID().getName().toUpperCase();
        switch (lType) {
        case "URI":
        case "IP":
            return "STRING";
        case "UINT64":
        case "UINT32":
        case "UINT16":
        case "UINT8":
        case "MAC":
            return "U64";
        default:
            return lType;
        }
    }

    public static boolean isPolicy(MClass aIn)
    {
        return aIn.isSubclassOf("policy/Component") || aIn.isSubclassOf("policy/Definition");
    }

    public static boolean isObservable(MClass aIn)
    {
        return aIn.isSubclassOf("observer/Component") || aIn.isSubclassOf("observer/Definition"); //TODO: WHAT SHOULD THESE CLASSES BE?
    }

    public static boolean isRelationshipSource(MClass aIn)
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

    public static  MNameRule getNamingRule(MClass aInClass)
    {
        Collection<MNameRule> lNrs = aInClass.findNamingRules();
        return lNrs.isEmpty() ? null : lNrs.iterator().next();
    }

    public static String getOwner(MClass aIn)
    {
        Collection<MOwner> lOwners = aIn.findOwners();
        return lOwners.isEmpty() ? (aIn.isConcrete() ? "default" : "abstract") : lOwners.iterator().next().getLID().getName();
    }

    private void genMo(int aInIndent, MClass aInClass)
    {
        out.println(aInIndent, '(');
            /**
            out.println(aInIndent, "// NAME: " + aInClass.getGID().getName());
            out.println(aInIndent, "// CONT PATHS: " + aInClass.getContainmentPaths());
            out.println(aInIndent, "// NAMING PATHS: " + aInClass.getNamingPaths());
            if (aInClass instanceof MRelationshipClass)
            {
                out.printIncodeComment(aInIndent + 1, new String[]
                        {
                                "RELATIONSHIPS: " + ((MRelationshipClass) aInClass).getRelationships(),
                                "NAME: " + aInClass.getGID().getName(),
                                "CONTAINED BY: " + aInClass.getContainedByClasses(true, true),
                                "SOURCE: " + ((MRelationshipClass) aInClass).getSourceClass().getGID().getName(),
                                "TARGETS: " + ((MRelationshipClass) aInClass).getTargetClasses(),
                        });
            }
             */
            out.print(aInIndent + 1, "ClassInfo(" + aInClass.getGID().getId() + ", ");

            // if (aInClass.hasSuperclass())
            // {
            //    MClass lSuper = aInClass.getSuperclass();
            //    out.print(lSuper.getGID().getId() + "/* super: " + lSuper.getGID().getName() + " */, ");
            // }
            // else
            // {
            //    out.print("0 /* no superclass */, ");
            // }
            out.println(getClassType(aInClass) + ", \"" + aInClass.getFullConcatenatedName() + "\", \"" + getOwner(aInClass) + "\",");
                genProps(aInIndent + 2, aInClass);
                genNamingProps(aInIndent + 2, aInClass);
                out.println(aInIndent + 2, ")");
        out.println(aInIndent, ')');
    }

    private void genProps(int aInIndent, MClass aInClass)
    {
        //boolean hasDesc = aInClass.hasProps() || aInClass.hasContained();
        TreeMap<String,MProp> lProps = new TreeMap<String, MProp>();
        aInClass.findProp(lProps,true);

        TreeMap<Ident,MClass> lConts = new TreeMap<Ident, MClass>();
        aInClass.getContainsClasses(lConts, true, true);//false, true);

        if (lProps.size() + lConts.size() == 0)
        {
            out.println(aInIndent, "std::vector<prop_id_t>(),");
        }
        else
        {
//            int lCount = 0;
            out.println(aInIndent, "list_of");
            // HANLDE PROPS
                for (MProp lProp : lProps.values())
                {
                    MType lPropType = lProp.getType(false);
                    MType lPrimitiveType = lPropType.getBuiltInType();
                    //MTypeHint lHint = lPrimitiveType.getTypeHint();

                    int lLocalId = lProp.getPropId();

                    if (isRelationshipSource(aInClass)) {
                        if (lProp.getLID().getName().equalsIgnoreCase("targetClass"))
                            continue;
                        else if (lProp.getLID().getName().equalsIgnoreCase("targetName")) {
                            out.println(aInIndent + 1,
                                        "(PropertyInfo(" + lLocalId + ", \"target\", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // " + lProp.toString());
                        }
                    }
                    else if (lProp.getLID().getName().equalsIgnoreCase("source") && isRelationshipTarget(aInClass))
                    {
                        //MClass lTargetClass = ((MRelationshipClass) aInClass).getSourceClass();
                        out.println(
                                aInIndent + 1,
                                "(PropertyInfo(" + lLocalId + ", \"" + lProp.getLID().getName() + "\", PropertyInfo::" + getTypeName(lPrimitiveType) + ", PropertyInfo::SCALAR)) // "
                                + lProp.toString());
                    }
                    // TODO
                    /**else if (isRelationshipResolver(aInClass))
                    {

                    }
                     **/
                    else
                    {
                        out.println(
                                aInIndent + 1,
                                "(PropertyInfo(" + lLocalId + ", \"" + lProp.getLID().getName() + "\", PropertyInfo::" + getTypeName(lPrimitiveType) + ", PropertyInfo::SCALAR)) // "
                                + lProp.toString());
                    }
                }


            // HANDLE CONTAINED CLASSES
                for (MClass lContained : lConts.values())
                {
                    out.println(aInIndent + 1, "(PropertyInfo(" + (1000 + lContained.getGID().getId()) + ", \"" + lContained.getFullConcatenatedName() + "\", PropertyInfo::COMPOSITE, " + lContained.getGID().getId() + ", PropertyInfo::VECTOR)) // " + lContained.toString());
                }

                out.println(aInIndent + 1, ",");
        }
    }

    private void genNamingProps(int aInIndent, MClass aInClass)
    {
        MNameRule lNr = getNamingRule(aInClass);

        if (null == lNr)
        {
            out.println(aInIndent, "std::vector<prop_id_t>() // no naming rule; assume cardinality of 1 in any containment rule");
        }
        else
        {
            Collection<MNameComponent> lComps = lNr.getComponents();
            int lNamePropsCount = 0;
            for (MNameComponent lIt : lComps)
            {
                if (lIt.hasPropName())
                {
                    lNamePropsCount++;
                }
            }
            if (0 == lNamePropsCount)
            {
                out.println(aInIndent, "std::vector<prop_id_t>() // no naming props in rule " + lNr + "; assume cardinality of 1");
            }
            else
            {
                out.println(aInIndent, "list_of // " + lNr);
                for (MNameComponent lIt : lComps)
                {
                    if (lIt.hasPropName())
                    {
                        MProp lProp = aInClass.findProp(lIt.getPropName(),false);
                        if (null != lProp)
                        {
                            out.println(aInIndent + 1, "(" + lProp.getPropId() + ") //" + lProp + " of name component " + lIt);
                        }
                    }
                }
            }
        }
    }
    //private TreeMap<String, Integer> propIds = new TreeMap<String, Integer>();
}
