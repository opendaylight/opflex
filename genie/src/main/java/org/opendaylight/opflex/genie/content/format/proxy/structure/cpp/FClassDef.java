package org.opendaylight.opflex.genie.content.format.proxy.structure.cpp;

import org.opendaylight.opflex.genie.content.format.agent.meta.cpp.FMetaDef;
import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.mnaming.MNameComponent;
import org.opendaylight.opflex.genie.content.model.mnaming.MNameRule;
import org.opendaylight.opflex.genie.content.model.module.Module;
import org.opendaylight.opflex.genie.content.model.mprop.MProp;
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
        // doesn't seem to be needed for now.  Revisit?
        //genForwardDecls(aInIndent, aInClass);
        genIncludes(aInIndent, aInClass);
        genBody(aInIndent, aInClass);
        out.println(aInIndent,"#endif // " + lInclTag);

    }

    private void genForwardDecls(int aInIndent, MClass aInClass)
    {
        out.println();
        out.println(aInIndent, "namespace " + Config.getProjName() + " {");
        out.println();
        out.printIncodeComment(aInIndent, "FORWARD DECLARATIONS FOR INHERITANCE ");
        for (MClass lThis = aInClass; null != lThis; lThis = lThis.getSuperclass())
        {
            genForwardDecl(aInIndent, lThis);
        }
        TreeMap<Ident, MClass> lConts = new TreeMap<Ident, MClass>();
        aInClass.getContainsClasses(lConts, true, true);
        out.printIncodeComment(aInIndent, "FORWARD DECLARATIONS FOR CONTAINED ");
        for (MClass lThis : lConts.values())
        {
            genForwardDecl(aInIndent, lThis);
        }
        TreeMap<Ident, MClass> lContr = new TreeMap<Ident, MClass>();
        aInClass.getContainedByClasses(lContr, true, true);
        out.printIncodeComment(aInIndent, "FORWARD DECLARATIONS FOR CONTAINERS ");
        for (MClass lThis : lContr.values())
        {
            genForwardDecl(aInIndent,lThis);
        }
        out.println();
        out.println(aInIndent, "} // namespace " + Config.getProjName());
        out.println();
    }

    private void genForwardDecl(int aInIndent, MClass aInClass)
    {
        String lNs = getNamespace(aInClass, false);

        out.println(aInIndent, "namespace " + lNs + " {");
        out.println();
        out.printHeaderComment(aInIndent, "forward declaration for " + aInClass);
        out.println(aInIndent, "class " + aInClass.getLID().getName() + ";");
        out.println();
        out.println(aInIndent, "} // namespace " + lNs);
        out.println();
    }

    private void genIncludes(int aInIndent, MClass aInClass)
    {
        out.println();
        out.println(aInIndent,getInclude("boost/optional.hpp", true));
        out.println(aInIndent,getInclude("opflex/modb/URIBuilder.h", false));
        out.println(aInIndent,getInclude("opflex/modb/mo-internal/MO.h", false));
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
            out.println(aInIndent + 1, ": public opflex::modb::mointernal::MO");
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
            return "(" + aInSyntax + ")";
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

        {
            genPropCheck(aInIndent,aInClass,aInProp,aInPropIdx,lType,lBaseType,lComments);
            genPropAccessor(aInIndent, aInClass, aInProp, aInPropIdx, lType, lBaseType, lComments);
            genPropMutator(aInIndent, aInClass, aInProp, aInPropIdx, lType, lBaseType, lComments);
        }
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
        out.println(aInIndent + 1, "return isPropertySet(" + toUnsignedStr(aInPropIdx) + ";");
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

    private void genPropAccessor(
            int aInIndent, MClass aInClass, MProp aInProp, int aInPropIdx, MType aInType, MType aInBaseType,
            Collection<String> aInComments)
    {
        String lName = aInProp.getLID().getName();
        String lPType = Strings.upFirstLetter(aInBaseType.getLID().getName());
        String lEffSyntax = getPropEffSyntax(aInBaseType);
        String lCast = getCast(lPType, lEffSyntax);
        lPType = getTypeAccessor(lPType);
        genPropAccessor(aInIndent, aInClass, aInProp, aInPropIdx, aInType, aInBaseType, aInComments,
                        lName, lName, lEffSyntax, lPType, lCast, "");
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
        out.println(aInIndent,"boost::optional<" + aInEffSyntax + "> get" + Strings.upFirstLetter(aInName) + "()");
        out.println(aInIndent,"{");
        out.println(aInIndent + 1,"if (is" + Strings.upFirstLetter(aInCheckName) + "Set())");
        out.println(aInIndent + 2,"return " + aInCast + "getProperty(" + toUnsignedStr(aInPropIdx) + ");");
        out.println(aInIndent + 1,"return boost::none;");
        out.println(aInIndent,"}");
        out.println();
    }

    private void genPropDefaultedAccessor(
            int aInIndent, MClass aInClass, MProp aInProp, int aInPropIdx, MType aInType, MType aInBaseType,
            Collection<String> aInComments, String aInName, String aInEffSyntax)
    {
        //
        // COMMENT
        //
        int lCommentSize = 3 + aInComments.size();
        String lComment[] = new String[lCommentSize];
        int lCommentIdx = 0;
        lComment[lCommentIdx++] = "Get the value of " + aInName + " if set, otherwise the value of default passed in.";

        for (String lCommLine : aInComments)
        {
            lComment[lCommentIdx++] = lCommLine;
        }
        //
        // DEF
        //
        lComment[lCommentIdx++] = "@param defaultValue default value returned if the property is not set";
        lComment[lCommentIdx++] = "@return the value of " + aInName + " if set, otherwise the value of default passed in";
        out.printHeaderComment(aInIndent,lComment);
        out.println(aInIndent, aInEffSyntax + " get" + Strings.upFirstLetter(aInName) + "(" + aInEffSyntax + " defaultValue)");
        //
        // BODY
        //
        out.println(aInIndent,"{");
        out.println(aInIndent + 1, "return get" + Strings.upFirstLetter(aInName) + "().get_value_or(defaultValue);");
        out.println(aInIndent,"}");
        out.println();
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
        lComment.add("@throws std::logic_error if no mutator is active");
        lComment.add("@see opflex::modb::Mutator");
        out.printHeaderComment(aInIndent,lComment);
        //
        // DEF
        //
        out.println(aInIndent,  getClassName(aInClass, true) + "& set" + Strings.upFirstLetter(aInName) + "(" + aInEffSyntax + " " + aInParamName + ")");
        //
        // BODY
        //
        out.println(aInIndent,"{");
        out.println(aInIndent + 1, "setProperty(" + toUnsignedStr(aInPropIdx)+ ", " + aInParamName + ");");
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
                "Set " + lName + " to the specified value in the currently-active mutator.");
        genPropMutator(aInIndent, aInClass, aInProp, aInPropIdx, aInType, aInBaseType,
                       lComments, aInComments, lName, lPType,
                       getPropEffSyntax(aInBaseType), "newValue", "the new value to set.",
                       "");
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

    private static String getResolverMethName(List<Pair<String, MNameRule>> aInNamingPath,
            boolean aInIsUniqueNaming)
    {
        return getMethName(aInNamingPath, aInIsUniqueNaming, "resolve");
    }

    private static String getRemoverMethName(List<Pair<String, MNameRule>> aInNamingPath,
            boolean aInIsUniqueNaming)
    {
        return getMethName(aInNamingPath, aInIsUniqueNaming, "remove");
    }

    public static int countNamingProps(List<Pair<String, MNameRule>> aInNamingPath)
    {
        int lRet = 0;
        for (Pair<String, MNameRule> lNode : aInNamingPath)
        {
            Collection<MNameComponent> lNcs = lNode.getSecond().getComponents();
            if (!(null == lNcs || lNcs.isEmpty()))
            {
                for (MNameComponent lNc : lNcs)
                {
                    if (lNc.hasPropName())
                    {
                        lRet++;
                    }
                }
            }
        }
        return lRet;
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

        String lSyntax = aInBaseType.getLanguageBinding(Language.CPP).getSyntax();
        if (lPassAsConst)
        {
            lRet.append("const ");
        }
        lRet.append(lSyntax);

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
            out.println(aInIdent + 1, "const opflex::modb::URI& uri)");
            out.println(aInIdent + 1, ": MO(CLASS_ID, uri) { }");
        }
    }
}
