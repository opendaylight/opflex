package org.opendaylight.opflex.genie.content.format.proxy.structure.cpp;

import org.opendaylight.opflex.genie.content.format.agent.meta.cpp.FMetaDef;
import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.mnaming.MNameComponent;
import org.opendaylight.opflex.genie.content.model.mnaming.MNameRule;
import org.opendaylight.opflex.genie.content.model.mnaming.MNamer;
import org.opendaylight.opflex.genie.content.model.module.Module;
import org.opendaylight.opflex.genie.content.model.mprop.MProp;
import org.opendaylight.opflex.genie.content.model.mrelator.MRelationshipClass;
import org.opendaylight.opflex.genie.content.model.mtype.Language;
import org.opendaylight.opflex.genie.content.model.mtype.MLanguageBinding;
import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.content.model.mtype.PassBy;
import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.*;
import org.opendaylight.opflex.genie.engine.model.Ident;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.proc.Config;
import org.opendaylight.opflex.modlan.report.Severity;
import org.opendaylight.opflex.modlan.utils.Strings;

import java.util.*;

/**
 * Created by midvorki on 11/20/14.
 */
public class FClassDef extends ItemFormatterTask
{
    public FClassDef(
            FormatterCtx aInFormatterCtx,
            FileNameRule aInFileNameRule,
            Indenter aInIndenter,
            BlockFormatDirective aInHeaderFormatDirective,
            BlockFormatDirective aInCommentFormatDirective,
            boolean aInIsUserFile,
            WriteStats aInStats,
            Item aInItem)
    {
        super(
                aInFormatterCtx,
                aInFileNameRule,
                aInIndenter,
                aInHeaderFormatDirective,
                aInCommentFormatDirective,
                aInIsUserFile,
                aInStats,
                aInItem);
    }

    /**
     * Optional API required by framework to regulate triggering of tasks.
     * This method identifies whether this task should be triggered for the item passed in.
     * @param aIn item for which task is considered to be trriggered
     * @return true if task shouold be triggered, false if task should be skipped for this item.
     */
    public static boolean shouldTriggerTask(Item aIn)
    {
        return ((MClass)aIn).isConcrete();
    }

    /**
     * Optional API required by framework to identify namespace/module-space for the corresponding generated file.
     * @param aIn item for which task is being triggered
     * @return name of the module/namespace that corresponds to the item
     */
    public static String getTargetModule(Item aIn)
    {
        return ((MClass) aIn).getModule().getLID().getName();
    }

    /**
     * Optional API required by the framework for transformation of file naming rule for the corresponding
     * generated file. This method can customize the location for the generated file.
     * @param aInFnr file name rule template
     * @param aInItem item for which file is generated
     * @return transformed file name rule
     */
    public static FileNameRule transformFileNameRule(FileNameRule aInFnr,Item aInItem)
    {
        String lTargetModue = getTargetModule(aInItem);
        String lOldRelativePath = aInFnr.getRelativePath();
        String lNewRelativePath = lOldRelativePath + "/include/" + Config.getProjName() + "/" + lTargetModue;

        FileNameRule lFnr = new FileNameRule(
                lNewRelativePath,
                null,
                aInFnr.getFilePrefix(),
                aInFnr.getFileSuffix(),
                aInFnr.getFileExtension(),
                getClassName((MClass)aInItem, false));

        return lFnr;
    }

    public void generate()
    {
        MClass lClass = (MClass) getItem();
        generate(0, lClass);
    }

    public static String getClassName(MClass aInClass, boolean aInFullyQualified)
    {
        return aInFullyQualified ? (getNamespace(aInClass,true) + "::" + aInClass.getLID().getName()) : aInClass.getLID().getName();
    }

    public static String getInclTag(MClass aInClass)
    {
        return "GI_" + aInClass.getModule().getLID().getName().toUpperCase() + '_' + aInClass.getLID().getName().toUpperCase() + "_HPP";
    }

    public static String getNamespace(String aInModuleName, boolean aInFullyQualified)
    {
        return aInFullyQualified ? (Config.getProjName() + "::" + aInModuleName) : (aInModuleName);
    }

    public static String getNamespace(MClass aInClass, boolean aInFullyQualified)
    {
        return getNamespace(aInClass.getModule().getLID().getName(), aInFullyQualified);
    }

    public static String getInclude(String aInPath, boolean aInIsBuiltIn)
    {
        return  "#include " + (aInIsBuiltIn ? '<' : '\"') + aInPath + (aInIsBuiltIn ? '>' : '\"');
    }

    public static String getIncludePath(MClass aIn)
    {
        return Config.getProjName() + "/" + getNamespace(aIn,false) + "/" + aIn.getLID().getName();
    }

    public static String getInclude(MClass aIn)
    {
        return getInclude(getIncludePath(aIn) + ".hpp", false);
    }
    private void generate(int aInIndent, MClass aInClass)
    {
        String lInclTag = getInclTag(aInClass);
        out.println(aInIndent,"#pragma once");
        out.println(aInIndent,"#ifndef " + lInclTag);
        out.println(aInIndent,"#define " + lInclTag);
        genIncludes(aInIndent, aInClass);
        genBody(aInIndent, aInClass);
        out.println(aInIndent,"#endif // " + lInclTag);

    }

    private void genIncludes(int aInIndent, MClass aInClass)
    {
        out.println();
        out.println(aInIndent,getInclude("boost/optional.hpp", true));
        out.println(aInIndent,getInclude("boost/pointer_cast.hpp", true));
        out.println(aInIndent,getInclude("opflex/modb/URIBuilder.h", false));
        out.println(aInIndent, getInclude("opflex/modb/MO.h", false));
        /**
         if (aInClass.hasSuperclass())
         {
         MClass lSuper = aInClass.getSuperclass();
         out.printIncodeComment(aInIndent, "superclass: " + lSuper);
         out.println(aInIndent,getInclude(lSuper), false);
         }
         */
        TreeMap<Ident, MClass> lConts = new TreeMap<Ident, MClass>();
        aInClass.getContainsClasses(lConts, true, true);
        for (MClass lThis : lConts.values())
        {
            out.printIncodeComment(aInIndent, "contains: " + lThis);
            out.println(aInIndent, getInclude(lThis), false);
        }
    }

    private void genBody(int aInIndent, MClass aInClass)
    {
        out.println();
        String lNs = getNamespace(aInClass, false);

        out.println(aInIndent, "namespace " + Config.getProjName() + " {");
        out.println(aInIndent, "namespace " + lNs + " {");
        out.println();
        genClass(aInIndent, aInClass);
        out.println();
        out.println(aInIndent, "} // namespace " + lNs);
        out.println(aInIndent, "} // namespace " + Config.getProjName());
    }

    private void genClass(int aInIndent, MClass aInClass)
    {
        out.println(aInIndent, "class " + aInClass.getLID().getName());
        /**
         if (aInClass.hasSuperclass())
         {
         MClass lSuperclass = aInClass.getSuperclass();
         out.println(aInIndent + 1, ": public " + getClassName(lSuperclass,true));
         }
         else**/
        {
            out.println(aInIndent + 1, ": public opflex::modb::MO");
        }
        out.println(aInIndent, "{");
        genPublic(aInIndent + 1, aInClass);
        out.println(aInIndent, "}; // class " + aInClass.getLID().getName());
    }

    private void genPublic(int aInIndent, MClass aInClass)
    {
        out.println(aInIndent - 1, "public:");
        out.println();
        genClassId(aInIndent, aInClass);
        genProps(aInIndent, aInClass);
        genResolvers(aInIndent, aInClass);
        genConstructor(aInIndent, aInClass);
    }

    private void genClassId(int aInIndent, MClass aInClass)
    {
        String lClassName = getClassName(aInClass, false);
        out.printHeaderComment(aInIndent, Arrays.asList("The unique class ID for " + lClassName));
        out.println(aInIndent, "static const opflex::modb::class_id_t CLASS_ID = " + aInClass.getGID().getId() + ";");
        out.println();
    }

    private void genProps(int aInIndent, MClass aInClass)
    {
        TreeMap<String, MProp> lProps = new TreeMap<String, MProp>();
        aInClass.findProp(lProps, true); // false
        for (MProp lProp : lProps.values())
        {
            // ONLY IF THIS PROPERTY IS DEFINED LOCALLY
            //if (lProp.getBase().getMClass() == aInClass)
            {
                genProp(aInIndent, aInClass, lProp, lProp.getPropId(aInClass));
            }
        }
    }

    public static String getTypeAccessor(String aInPType)
    {
        if (aInPType.startsWith("Enum") ||
            aInPType.startsWith("Bitmask") ||
            aInPType.startsWith("UInt"))
        {
            aInPType = "UInt64";
        }
        else if (aInPType.equals("URI") ||
                 aInPType.equals("IP") ||
                 aInPType.equals("UUID"))
        {
            aInPType = "String";
        }
        return aInPType;
    }

    public static String getCast(String aInPType, String aInSyntax)
    {
        if (aInPType.startsWith("Enum") ||
            aInPType.startsWith("Bitmask") ||
            (aInPType.startsWith("UInt") &&
             !aInPType.equals("UInt64")))
        {
            return aInSyntax;
        }
        return "";
    }

    private String toUnsignedStr(int aInInt)
    {
        return Long.toString(aInInt & 0xFFFFFFFFL) + "ul";
    }

    private void genProp(int aInIndent, MClass aInClass, MProp aInProp, int aInPropIdx)
    {
        MProp lBaseProp = aInProp.getBase();
        MType lType = lBaseProp.getType(false);
        MType lBaseType = lType.getBuiltInType();

        LinkedList<String> lComments = new LinkedList<String>();
        aInProp.getComments(lComments);

        if (aInClass.isConcreteSuperclassOf("relator/Source") &&
            aInProp.getLID().getName().toLowerCase().startsWith("target"))
        {
            if (aInProp.getLID().getName().equalsIgnoreCase("targetName"))
            {
                genRef(aInIndent,aInClass,aInProp,aInPropIdx,lType,lBaseType,lComments, true);
            }
        }
        else
        {
            genPropCheck(aInIndent,aInClass,aInProp,aInPropIdx,lType,lBaseType,lComments);
            genPropAccessor(aInIndent, aInClass, aInProp, aInPropIdx, lType, lBaseType, lComments);
            genPropMutator(aInIndent, aInClass, aInProp, aInPropIdx, lType, lBaseType, lComments);
        }
    }

    private void genRef(int aInIndent, MClass aInClass, MProp aInProp, int aInPropIdx, MType aInType, MType aInBaseType,
            Collection<String> aInComments, boolean aInTarget)
    {
        genRefCheck(aInIndent, aInClass, aInProp, aInPropIdx, aInType, aInType, aInComments, aInTarget);
        genRefAccessors(aInIndent, aInClass, aInProp, aInPropIdx, aInType, aInType, aInComments, aInTarget);
        genRefMutators(aInIndent, aInClass, aInProp, aInPropIdx, aInType, aInType, aInComments, aInTarget);
    }

    private void genPropCheck(
            int aInIndent, MClass aInClass, MProp aInProp, int aInPropIdx, MType aInType, MType aInBaseType,
            Collection<String> aInComments, String aInCheckName, String aInPType)
    {
        //
        // COMMENT
        //
        int lCommentSize = 2 + aInComments.size();
        String lComment[] = new String[lCommentSize];
        int lCommentIdx = 0;
        lComment[lCommentIdx++] = "Check whether " + aInCheckName + " has been set";
        for (String lCommLine : aInComments)
        {
            lComment[lCommentIdx++] = lCommLine;
        }
        lComment[lCommentIdx++] = "@return true if " + aInCheckName + " has been set";
        out.printHeaderComment(aInIndent,lComment);
        //
        // METHOD DEFINITION
        //
        out.println(aInIndent,"bool is" + Strings.upFirstLetter(aInCheckName) + "Set()");

        //
        // METHOD BODY
        //
        out.println(aInIndent,"{");
        out.println(aInIndent + 1, "return isPropertySet(" + toUnsignedStr(aInPropIdx) + ");");
        out.println(aInIndent,"}");
        out.println();
    }

    private void genPropCheck(
            int aInIndent, MClass aInClass, MProp aInProp, int aInPropIdx, MType aInType, MType aInBaseType,
            Collection<String> aInComments)
    {
        String lPType = FMetaDef.getTypeName(aInBaseType);
        genPropCheck(aInIndent, aInClass, aInProp, aInPropIdx, aInType, aInBaseType,
                     aInComments, aInProp.getLID().getName(),
                     lPType);
    }

    private void genRefCheck(
            int aInIndent, MClass aInClass, MProp aInProp, int aInPropIdx, MType aInType, MType aInBaseType,
            Collection<String> aInComments, boolean target)
    {
        genPropCheck(aInIndent, aInClass, aInProp, aInPropIdx, aInType, aInBaseType,
                     aInComments, target ? "target" : "source", "REFERENCE");
    }

    private void genPropAccessor(
            int aInIndent, MClass aInClass, MProp aInProp, int aInPropIdx, MType aInType, MType aInBaseType,
            Collection<String> aInComments, String aInCheckName, String aInName, String aInEffSyntax, String aInPType,
            String aInCast, String aInAccessor)
    {
        //
        // COMMENT
        //
        int lCommentSize = 2 + aInComments.size();
        String lComment[] = new String[lCommentSize];
        int lCommentIdx = 0;
        lComment[lCommentIdx++] = "Get the value of " + aInName + " if it has been set.";

        for (String lCommLine : aInComments)
        {
            lComment[lCommentIdx++] = lCommLine;
        }
        lComment[lCommentIdx++] = "@return the value of " + aInName + " or boost::none if not set";
        out.printHeaderComment(aInIndent,lComment);
        String lReturnType = aInCast.isEmpty() ? aInEffSyntax : aInCast;
        out.println(aInIndent, "boost::optional<" + lReturnType + "> get" + Strings.upFirstLetter(aInName) + "()");
        out.println(aInIndent, "{");
        String lCast = aInCast.isEmpty() ? "" : "(" + aInCast + ")";
        out.println(aInIndent + 1, "if (is" + Strings.upFirstLetter(aInCheckName) + "Set())");
        if (aInPType.equals("Reference"))
        {
            aInEffSyntax = "opflex::modb::reference_t";
        }
        out.println(aInIndent + 2,"return " + lCast + "boost::get<" + aInEffSyntax + ">(getProperty(" + toUnsignedStr(aInPropIdx) + "))" + aInAccessor + ";");
        out.println(aInIndent + 1,"return boost::none;");
        out.println(aInIndent,"}");
        out.println();
    }

    private void genPropAccessor(
            int aInIndent, MClass aInClass, MProp aInProp, int aInPropIdx, MType aInType, MType aInBaseType,
            Collection<String> aInComments)
    {
        String lName = aInProp.getLID().getName();
        String lPType = Strings.upFirstLetter(aInBaseType.getLID().getName());
        String lEffSyntax = getPropEffSyntax(aInBaseType);
        String lCast = getCast(lPType, lEffSyntax);
        lEffSyntax = mapToInternalRep(aInBaseType, lEffSyntax);
        lPType = getTypeAccessor(lPType);
        genPropAccessor(aInIndent, aInClass, aInProp, aInPropIdx, aInType, aInBaseType, aInComments,
                        lName, lName, lEffSyntax, lPType, lCast, "");
    }

    private void genRefAccessors(
            int aInIndent, MClass aInClass, MProp aInProp, int aInPropIdx, MType aInType, MType aInBaseType,
            Collection<String> aInComments, boolean target)
    {
        String lName = (target ? "target" : "source");
        genPropAccessor(aInIndent, aInClass, aInProp, aInPropIdx, aInType, aInBaseType, aInComments,
                        lName, lName + "Class", "opflex::modb::class_id_t", "Reference", "", ".first");
        genPropAccessor(aInIndent, aInClass, aInProp, aInPropIdx, aInType, aInBaseType, aInComments,
                        lName, lName + "URI", "opflex::modb::URI", "Reference", "", ".second");
    }

    private void genPropMutator(int aInIndent, MClass aInClass, MProp aInProp, int aInPropIdx, MType aInType, MType aInBaseType,
            Collection<String> aInBaseComments, Collection<String> aInComments, String aInName, String aInPType, String aInEffSyntax, String aInParamName, String aInParamHelp,
            String aInSetterPrefix)
    {
        //
        // COMMENT
        //
        ArrayList<String> lComment = new ArrayList<>(aInBaseComments);
        if (aInComments.size() > 0) lComment.add("");
        lComment.addAll(aInComments);
//        Arrays.asList(
 //
        lComment.add("");
        lComment.add("@param " + aInParamName + " " + aInParamHelp);
        lComment.add("@return a reference to the current object");
        out.printHeaderComment(aInIndent,lComment);
        //
        // DEF
        //
        out.println(aInIndent,  getClassName(aInClass, true) + "& set" + Strings.upFirstLetter(aInName) + "(" + aInEffSyntax + " " + aInParamName + ")");
        //
        // BODY
        //
        out.println(aInIndent,"{");
        if (!aInSetterPrefix.isEmpty())
        { //<opflex::modb::class_id_t, opflex::modb::URI>
            out.println(aInIndent + 1, "setProperty(" + toUnsignedStr(aInPropIdx) + ", (opflex::modb::reference_t)std::make_pair(" + aInSetterPrefix + ", "  + aInParamName + "));");
        }
        else
        {
            String lCast = mapToInternalRep(aInBaseType, aInEffSyntax);
            lCast = lCast.equals(aInEffSyntax) ? "" : "(" + lCast + ")";
            out.println(aInIndent + 1, "setProperty(" + toUnsignedStr(aInPropIdx) + ", " + lCast + aInParamName + ");");
        }
        out.println(aInIndent + 1, "return *this;");
        out.println(aInIndent,"}");
        out.println();
    }

    private void genNamedPropMutators(int aInIndent, MClass aInClass, MClass aInRefClass, int aInPropIdx,
                                      List<Pair<String, MNameRule>> aInNamingPath, boolean aInIsUniqueNaming,
                                      String aInMethName, String aInPType, String aInSetterPrefix)
    {
        String lRefClassName = getClassName(aInRefClass, false);
        ArrayList<String> lComment = new ArrayList<>(Arrays.asList(
            "Set the reference to point to an instance of " + lRefClassName,
            "by constructing its URI from the path elements that lead to it.",
            "",
            "The reference URI generated by this function will take the form:",
            getUriDoc(aInRefClass, aInNamingPath),
            ""));
        addPathComment(aInClass, aInNamingPath, lComment);
        lComment.add("");
        lComment.add("@return a reference to the current object");
        out.printHeaderComment(aInIndent,lComment);

        //
        // DEF
        //
        String lMethName = getMethName(aInNamingPath, aInIsUniqueNaming, aInMethName);
        out.println(aInIndent,  getClassName(aInClass, true) + "& set" + Strings.upFirstLetter(lMethName) + "(");
        boolean lIsFirst = true;
        for (Pair<String,MNameRule> lNamingNode : aInNamingPath)
        {
            MNameRule lNr = lNamingNode.getSecond();
            MClass lThisContClass = MClass.get(lNamingNode.getFirst());

            Collection<MNameComponent> lNcs = lNr.getComponents();
            for (MNameComponent lNc : lNcs)
            {
                if (lNc.hasPropName())
                {
                    if (lIsFirst)
                    {
                        lIsFirst = false;
                    }
                    else
                    {
                        out.println(",");
                    }
                    out.print(aInIndent + 1, getPropParamDef(lThisContClass, lNc.getPropName()));
                }
            }
        }
        if (lIsFirst)
        {
            out.println(aInIndent + 1, ")");
        }
        else
        {
            out.println(")");
        }
        //
        // BODY
        //
        String lUriBuilder = getUriBuilder(aInClass, aInNamingPath);
        out.println(aInIndent,"{");
        out.println(aInIndent + 1, "set" + Strings.upFirstLetter(lMethName) + "(" + lUriBuilder + ");");
        out.println(aInIndent + 1, "return *this;");
        out.println(aInIndent,"}");
        out.println();
    }

    private void genPropMutator(int aInIndent, MClass aInClass, MProp aInProp, int aInPropIdx, MType aInType, MType aInBaseType,
            Collection<String> aInComments)
    {
        String lName = aInProp.getLID().getName();
        String lPType = Strings.upFirstLetter(aInBaseType.getLID().getName());
        lPType = getTypeAccessor(lPType);
        List<String> lComments = Arrays.asList(
                "Set " + lName + " to the specified value.");
        genPropMutator(aInIndent, aInClass, aInProp, aInPropIdx, aInType, aInBaseType,
                       lComments, aInComments, lName, lPType,
                       getPropEffSyntax(aInBaseType), "newValue", "the new value to set.",
                       "");
    }

    private void genRefMutators(int aInIndent, MClass aInClass, MProp aInProp, int aInPropIdx, MType aInType, MType aInBaseType,
                                Collection<String> aInComments, boolean target)
    {
        for (MClass lTargetClass : ((MRelationshipClass) aInClass).getTargetClasses(true))
        {
            String lName = (target ? "target" : "source") +
                    Strings.upFirstLetter(lTargetClass.getLID().getName());
            List<String> lComments = Arrays.asList(
                    "Set the reference to point to an instance of " + getClassName(lTargetClass, false),
                    "with the specified URI");
            genPropMutator(aInIndent, aInClass, aInProp, aInPropIdx, aInType, aInBaseType,
                    lComments, aInComments, lName, "Reference",
                    "const opflex::modb::URI&", "uri", "The URI of the reference to add",
                    Integer.toString(lTargetClass.getGID().getId()));

            Collection<List<Pair<String, MNameRule>>> lNamingPaths = new LinkedList<List<Pair<String, MNameRule>>>();
            boolean lIsUniqueNaming = lTargetClass.getNamingPaths(lNamingPaths, Language.CPP);
            for (List<Pair<String, MNameRule>> lNamingPath : lNamingPaths)
            {
//                if (!hasValidPath(lNamingPath)) continue;
                genNamedPropMutators(aInIndent, aInClass, lTargetClass, aInPropIdx, lNamingPath, lIsUniqueNaming,
                        lName, "Reference", Integer.toString(lTargetClass.getGID().getId()));
            }
        }
    }


    private void genResolvers(int aInIdent, MClass aInClass)
    {
        if (aInClass.isRoot())
        {
            genRootCreation(aInIdent,aInClass);
        }
        genChildrenResolvers(aInIdent, aInClass);
    }

    private void genRootCreation(int aInIdent, MClass aInClass)
    {
        String lClassName = getClassName(aInClass, true);
        ArrayList<String> lComment = new ArrayList<>(Arrays.asList(
                "Create an instance of " + getClassName(aInClass, false) + ", the root element in the",
                "management information tree."));
        out.printHeaderComment(aInIdent,lComment);

        out.println(aInIdent, "static boost::shared_ptr<" + lClassName + "> createRootElement()");
        out.println(aInIdent,"{");
        out.println(aInIdent + 1, "return boost::shared_ptr<" + lClassName + ">(new " + lClassName + "(opflex::modb::URI::ROOT, -1, opflex::modb::URI::ROOT, -1));");
        out.println(aInIdent, "}");
        out.println();
    }

    private void genChildrenResolvers(int aInIdent, MClass aInClass)
    {
        TreeMap<Ident,MClass> lConts = new TreeMap<Ident, MClass>();
        aInClass.getContainsClasses(lConts, true, true);//true, true);
        for (MClass lChildClass : lConts.values())
        {
            genChildResolvers(aInIdent,aInClass,lChildClass);
        }
    }

    private void addPathComment(MClass aInThisContClass,
                                Collection<MNameComponent> aInNcs,
                                List<String> result)
    {
        String lclassName = getClassName(aInThisContClass, false);
        for (MNameComponent lNc : aInNcs)
        {
            if (lNc.hasPropName())
            {
                result.add("@param " +
                        getPropParamName(aInThisContClass, lNc.getPropName()) +
                        " the value of " +
                        getPropParamName(aInThisContClass, lNc.getPropName()) + ",");
                result.add("a naming property for " + lclassName);
            }
        }
    }

    private void addPathComment(MClass aInClass,
                                List<Pair<String, MNameRule>> aInNamingPath,
                                List<String> result)
    {
        for (Pair<String,MNameRule> lNamingNode : aInNamingPath)
        {
            MNameRule lNr = lNamingNode.getSecond();
            MClass lThisContClass = MClass.get(lNamingNode.getFirst());

            Collection<MNameComponent> lNcs = lNr.getComponents();
            addPathComment(lThisContClass, lNcs, result);
        }
    }

    private static String getMethName(List<Pair<String, MNameRule>> aInNamingPath,
            boolean aInIsUniqueNaming,
            String prefix)
    {
        if (aInIsUniqueNaming)
        {
            return prefix;
        }
        else
        {
            StringBuilder lSb = new StringBuilder();
            lSb.append(prefix + "Under");
            int lSize = aInNamingPath.size();
            int lIdx = lSize;
            for (Pair<String, MNameRule> lPathNode : aInNamingPath)
            {
                if (0 < --lIdx)
                {
                    MClass lClass = MClass.get(lPathNode.getFirst());
                    Module lMod = lClass.getModule();
                    lPathNode.getSecond();

                    lSb.append(Strings.upFirstLetter(lMod.getLID().getName()));
                    lSb.append(Strings.upFirstLetter(lClass.getLID().getName()));
                }
            }
            return lSb.toString();
        }
    }

    public static void getPropParamName(MClass aInClass, String aInPropName, StringBuilder aOutSb)
    {
        aOutSb.append(aInClass.getModule().getLID().getName());
        aOutSb.append(Strings.upFirstLetter(aInClass.getLID().getName()));
        aOutSb.append(Strings.upFirstLetter(aInPropName));
    }

    public static String getPropParamName(MClass aInClass, String aInPropName)
    {
        StringBuilder lSb = new StringBuilder();
        getPropParamName(aInClass,aInPropName,lSb);
        return lSb.toString();
    }

    public static String getPropEffSyntax(MType aInBaseType)
    {
        StringBuilder lRet = new StringBuilder();
        MLanguageBinding lLang = aInBaseType.getLanguageBinding(Language.CPP);
        boolean lPassAsConst = lLang.getPassConst();
        PassBy lPassBy = lLang.getPassBy();

        if (lPassAsConst)
        {
            lRet.append("const ");
        }
        lRet.append(lLang.getSyntax());

        switch (lPassBy)
        {
            case REFERENCE:
            case POINTER:

                lRet.append('&');
                break;

            case VALUE:
            default:

                break;
        }
        return lRet.toString();
    }

    public static String getPropParamDef(MClass aInClass, String aInPropName)
    {
        MProp lProp = aInClass.findProp(aInPropName, false);
        if (null == lProp)
        {
            Severity.DEATH.report(aInClass.toString(),
                                  "preparing param defs for prop: " + aInPropName,
                                  "no such property: " + aInPropName, "");
        }
        MProp lBaseProp = lProp.getBase();
        MType lType = lBaseProp.getType(false);
        MType lBaseType = lType.getBuiltInType();
        StringBuilder lRet = new StringBuilder();
        lRet.append(getPropEffSyntax(lBaseType));
        lRet.append(" ");
        getPropParamName(aInClass, aInPropName, lRet);
        return lRet.toString();
    }

    public static String getUriBuilder(MClass aInClass, List<Pair<String, MNameRule>> aInNamingPath)
    {
        StringBuilder lSb = new StringBuilder();
        getUriBuilder(aInClass,aInNamingPath, lSb);
        return lSb.toString();
    }

    public static void getUriBuilder(MClass aInClass, List<Pair<String, MNameRule>> aInNamingPath, StringBuilder aOut)
    {
        aOut.append("opflex::modb::URIBuilder()");
        for (Pair<String,MNameRule> lNamingNode : aInNamingPath)
        {
            MNameRule lNr = lNamingNode.getSecond();
            MClass lThisContClass = MClass.get(lNamingNode.getFirst());
            Collection<MNameComponent> lNcs = lNr.getComponents();
            aOut.append(".addElement(\"");

            aOut.append(lThisContClass.getFullConcatenatedName());
            aOut.append("\")");
            for (MNameComponent lNc : lNcs)
            {
                if (lNc.hasPropName())
                {
                    aOut.append(".addElement(");
                    getPropParamName(lThisContClass, lNc.getPropName(), aOut);
                    aOut.append(")");
                }
            }
        }
        aOut.append(".build()");
    }

    public static String getUriDoc(MClass aInClass, List<Pair<String, MNameRule>> aInNamingPath)
    {
        StringBuilder lSb = new StringBuilder();
        getUriDoc(aInClass,aInNamingPath, lSb);
        return lSb.toString();
    }
    public static void getUriDoc(MClass aInClass, List<Pair<String, MNameRule>> aInNamingPath, StringBuilder aOut)
    {
        for (Pair<String,MNameRule> lNamingNode : aInNamingPath)
        {
            MNameRule lNr = lNamingNode.getSecond();
            MClass lThisContClass = MClass.get(lNamingNode.getFirst());
            Collection<MNameComponent> lNcs = lNr.getComponents();
            aOut.append("/" + lThisContClass.getFullConcatenatedName());
            for (MNameComponent lNc : lNcs)
            {
                if (lNc.hasPropName())
                {
                    aOut.append("/[");
                    getPropParamName(lThisContClass, lNc.getPropName(), aOut);
                    aOut.append("]");
                }
            }
        }
    }

    public static String getUriBuilder(MClass aInParentClass,MClass aInChildClass, MNameRule aInNamingRule)
    {
        StringBuilder lSb = new StringBuilder();
        getUriBuilder(aInParentClass, aInChildClass, aInNamingRule, lSb);
        return lSb.toString();

    }
    public static void getUriBuilder(MClass aInParentClass,MClass aInChildClass, MNameRule aInNamingRule, StringBuilder aOut)
    {
        aOut.append("opflex::modb::URIBuilder(getURI())");
        aOut.append(".addElement(\"");
        aOut.append(aInChildClass.getFullConcatenatedName());
        aOut.append("\")");
        Collection<MNameComponent> lNcs = aInNamingRule.getComponents();
        for (MNameComponent lNc : lNcs)
        {
            if (lNc.hasPropName())
            {
                aOut.append(".addElement(");
                getPropParamName(aInChildClass, lNc.getPropName(), aOut);
                aOut.append(")");
            }
        }
        aOut.append(".build()");
    }

    private void genChildAdder(int aInIdent, MClass aInParentClass, MClass aInChildClass,
                               Collection<MNameComponent> aInNcs,
                               String aInFormattedChildClassName,
                               String aInConcatenatedChildClassName,
                               String aInUriBuilder,
                               MNameRule aInChildNr,
                               boolean aInMultipleChildren,
                               MClass aInTargetClass,
                               boolean aInTargetUnique)
    {

        ArrayList<String> comment = new ArrayList<>(Arrays.asList(
                "Create a new child object with the specified naming properties",
                "and make it a child of this object.",
                ""));
        addPathComment(aInChildClass, aInNcs, comment);
        comment.add("@return a pointer to the object");
        out.printHeaderComment(aInIdent,comment);
        String lTargetClassName = null;
        if (aInTargetClass != null)
            lTargetClassName = Strings.upFirstLetter(aInTargetClass.getLID().getName());
        String lMethodName = "add" + aInConcatenatedChildClassName;

        if (!aInTargetUnique && aInMultipleChildren && aInTargetClass != null)
        {
            lMethodName += lTargetClassName;
        }
        out.println(aInIdent, "boost::shared_ptr<" + aInFormattedChildClassName + "> " + lMethodName + "(");

        boolean lIsFirst = true;
        MNameComponent lClassProp = null;
        for (MNameComponent lNc : aInNcs)
        {
            if (lNc.hasPropName())
            {
                if (aInTargetClass != null)
                {
                    if (lNc.getPropName().equalsIgnoreCase("targetClass"))
                    {
                        lClassProp = lNc;
                        continue;
                    }
                }
                if (lIsFirst)
                {
                    lIsFirst = false;
                }
                else
                {
                    out.println(",");
                }
                out.print(aInIdent + 1, getPropParamDef(aInChildClass, lNc.getPropName()));
            }
        }
        if (lIsFirst)
        {
            out.println(aInIdent + 1, ")");
        }
        else
        {
            out.println(")");
        }
        out.println(aInIdent,"{");
        if (aInTargetClass != null && lClassProp != null)
        {
            out.println(aInIdent + 1, "opflex::modb::class_id_t " + getPropParamName(aInChildClass, lClassProp.getPropName()) + " = " + aInTargetClass.getGID().getId() + ";");
        }
        out.println(aInIdent + 1, "boost::shared_ptr<opflex::modb::MO> mo = addChild(");
        out.println(aInIdent + 2, toUnsignedStr(aInChildClass.getClassAsPropId(aInParentClass)) + ", " + aInChildClass.getGID().getId() + ",");
        out.println(aInIdent + 2, aInUriBuilder);
        out.println(aInIdent + 2, ");");
        out.println(aInIdent + 1, "boost::shared_ptr<" + aInFormattedChildClassName + "> result = boost::static_pointer_cast<" + aInFormattedChildClassName + ">(mo);");
        aInNcs = aInChildNr.getComponents();
        for (MNameComponent lNc : aInNcs)
        {
            if (lNc.hasPropName())
            {
                String lPropName = Strings.upFirstLetter(lNc.getPropName());
                String lArgs = getPropParamName(aInChildClass, lNc.getPropName());
                if (aInTargetClass != null) {
                    if (lPropName.equalsIgnoreCase("targetClass"))
                        continue;
                    if (lPropName.equalsIgnoreCase("targetName"))
                    {
                        lPropName = "Target" + lTargetClassName;
                        lArgs = "opflex::modb::URI(" + lArgs + ")";
                    }
                }
                out.println(aInIdent + 1,
                        "result->set" + lPropName +
                                "(" + lArgs + ");");
            }
        }
        out.println(aInIdent + 1, "return result;");
        out.println(aInIdent,"}");
        out.println();
    }

    private void genChildResolvers(int aInIdent, MClass aInParentClass, MClass aInChildClass)
    {
        MNamer lChildNamer = MNamer.get(aInChildClass.getGID().getName(),false);
        MNameRule lChildNr = lChildNamer.findNameRule(aInParentClass.getGID().getName());
        if (null != lChildNr)
        {
            String lFormattedChildClassName = getClassName(aInChildClass,true);
            String lConcatenatedChildClassName = aInChildClass.getFullConcatenatedName();
            String lUriBuilder = getUriBuilder(aInParentClass,aInChildClass, lChildNr);
            Collection<MNameComponent> lNcs = lChildNr.getComponents();
            boolean lMultipleChildren = false;
            for (MNameComponent lNc : lNcs)
            {
                if (lNc.hasPropName())
                {
                    lMultipleChildren = true;
                    break;
                }
            }
            if (aInChildClass.isConcreteSuperclassOf("relator/Source"))
            {
                Collection<MClass> lTargetClasses = ((MRelationshipClass) aInChildClass).getTargetClasses(true);
                for (MClass lTargetClass : lTargetClasses)
                {
                    genChildAdder(aInIdent, aInParentClass, aInChildClass, lNcs,
                            lFormattedChildClassName, lConcatenatedChildClassName,
                            lUriBuilder, lChildNr, lMultipleChildren, lTargetClass,
                            lTargetClasses.size() == 1);
                    if (!lMultipleChildren) break;
                }
            }
            else
            {
                genChildAdder(aInIdent, aInParentClass, aInChildClass, lNcs,
                        lFormattedChildClassName, lConcatenatedChildClassName,
                        lUriBuilder, lChildNr, lMultipleChildren, null, false);
            }
        }
        else
        {
            Severity.DEATH.report(aInParentClass.toString(), "child object resolver for " + aInChildClass.getGID().getName()," no naming rule", "");
        }
    }

    private void genConstructor(int aInIdent, MClass aInClass)
    {
        String lclassName = getClassName(aInClass, false);
        String[] lComment =
                {"Construct an instance of " + lclassName + ".",
                        "This should not typically be called from user code."};
        out.printHeaderComment(aInIdent,lComment);

        if (aInClass.isConcrete())
        {
            out.println(aInIdent, aInClass.getLID().getName() + "(");
            out.println(aInIdent + 1, "const opflex::modb::URI& uri,");
            out.println(aInIdent + 1, "const opflex::modb::class_id_t parent_class_id,");
            out.println(aInIdent + 1, "const opflex::modb::URI& parent_uri,");
            out.println(aInIdent + 1, "const opflex::modb::prop_id_t parent_prop)");
            out.println(aInIdent + 1, ": MO(CLASS_ID, uri, parent_class_id, parent_uri, parent_prop) { }");
        }
    }

    private String mapToInternalRep(MType aInBaseType, String lEffSyntax) {
        // need to adjust syntax to handle storing everything as 64-bit
        // required as the metadata drops more specific types (see property_type_t)
        if (aInBaseType.getLanguageBinding(Language.CPP).getSyntax().startsWith("uint"))
        {
            lEffSyntax = getPropEffSyntax(MType.get("scalar/UInt64"));
        }
        else if (aInBaseType.getLanguageBinding(Language.CPP).getSyntax().startsWith("int"))
        {
            lEffSyntax = getPropEffSyntax(MType.get("scalar/Int64"));
        }
        return lEffSyntax;
    }
}
