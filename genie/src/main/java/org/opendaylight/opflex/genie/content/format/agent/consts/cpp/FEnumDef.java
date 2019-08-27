package org.opendaylight.opflex.genie.content.format.agent.consts.cpp;

import org.opendaylight.opflex.genie.content.model.mconst.MConst;
import org.opendaylight.opflex.genie.content.model.mprop.MProp;
import org.opendaylight.opflex.genie.content.model.mtype.Language;
import org.opendaylight.opflex.genie.content.model.mtype.MLanguageBinding;
import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.*;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.proc.Config;
import org.opendaylight.opflex.modlan.utils.Strings;

import java.util.Collection;

/**
 * Created by midvorki on 11/5/14.
 */
public class FEnumDef extends ItemFormatterTask
{
    public FEnumDef(
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
     * @param aIn item for which task is considered to be triggered
     * @return true if task should be triggered, false if task should be skipped for this item.
     */
    public static boolean shouldTriggerTask(Item aIn)
    {
        if (aIn instanceof MProp)
        {
            return  ((MProp) aIn).hasEnumeratedConstants();
        }
        else if (aIn instanceof MType)
        {
            return  ((MType) aIn).hasEnumeratedConstants();
        }
        else
        {
            return false;
        }
    }

    /**
     * Optional API required by framework to identify namespace/module-space for the corresponding generated file.
     * @param aIn item for which task is being triggered
     * @return name of the module/namespace that corresponds to the item
     */
    public static String getTargetModule(Item aIn)
    {
        if (aIn instanceof MProp)
        {
            return  ((MProp) aIn).getModule().getLID().getName();
        }
        else if (aIn instanceof MType)
        {
            return  ((MType) aIn).getModule().getLID().getName();
        }
        else
        {
            return null;
        }
    }

    public static String getClassName(Item aIn, boolean aInIsFullyQualified)
    {
        return  (aInIsFullyQualified ? getTargetModule(aIn) : "") +
                (aIn instanceof MProp ?
                        Strings.upFirstLetter(((MProp) aIn).getMClass().getLID().getName()) :
                        "") +
               Strings.upFirstLetter(aIn.getLID().getName()) +
               "EnumT";
    }

    public static String getNamespace(String aInModuleName, boolean aInFullyQualified)
    {
        return aInFullyQualified ? (Config.getProjName() + "::" + aInModuleName) : (aInModuleName);
    }

    public static String getNamespace(Item aIn, boolean aInFullyQualified)
    {
        return getNamespace(getTargetModule(aIn), aInFullyQualified);
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
        String lNewRelativePath = aInFnr.getRelativePath() + "/include/" + Config.getProjName() + "/" + getTargetModule(aInItem);
        return new FileNameRule(
                lNewRelativePath,
                null,
                aInFnr.getFilePrefix(),
                aInFnr.getFileSuffix(),
                aInFnr.getFileExtension(),
                getClassName(aInItem, false));
    }

    public static Item getSuperHolder(Item aIn)
    {
        if (aIn instanceof MProp)
        {
            return ((MProp) aIn).getNextConstantHolder();
        }
        else if (aIn instanceof MType)
        {
            return ((MType) aIn).getNextConstantHolder();
        }
        else
        {
            return null;
        }
    }

    public static Collection<MConst> getConsts(Item aIn)
    {
        if (aIn instanceof MProp)
        {
            return ((MProp) aIn).getConst();
        }
        else if (aIn instanceof MType)
        {
            return ((MType) aIn).getConst();
        }
        else
        {
            return null;
        }
    }

    public static MType getType(Item aIn)
    {
        if (aIn instanceof MProp)
        {
            return ((MProp) aIn).getType(false);
        }
        else if (aIn instanceof MType)
        {
            return ((MType) aIn);
        }
        else
        {
            return null;
        }
    }

    public static String getInclTag(Item aInItem)
    {
        return "GI_" + getNamespace(aInItem,false).toUpperCase() + '_' + getClassName(aInItem, false).toUpperCase() + "_HPP";
    }

    public void generate()
    {
        genBody(0, getItem());
    }

    private void genBody(int aInIndent, Item aIn)
    {
        out.println(aInIndent,"#pragma once");
        String lInclTag = getInclTag(aIn);
        out.println(aInIndent,"#ifndef " + lInclTag);
        out.println(aInIndent,"#define " + lInclTag);

        Item lSuperHolder = getSuperHolder(aIn);

        if (null != lSuperHolder)
        {
            out.println(aInIndent, "#include \"" +  Config.getProjName() +
                                               "/" + getTargetModule(lSuperHolder) +
                                               "/" + getClassName(lSuperHolder, false)+ ".hpp\"");

        }
        else
        {
            out.println(aInIndent, "#include <cstdint>");
        }
        out.println(aInIndent, "namespace " + Config.getProjName() + " {");
        out.println(aInIndent, "namespace " + getNamespace(aIn,false) + " {");


        out.println(aInIndent + 1, "struct " + getClassName(aIn, false) +
                                   (null != lSuperHolder ?
                                            ": public " + getClassName(lSuperHolder,true) :
                                             "") + " {");

            MType lType = getType(aIn);
            MType lBaseType = lType.getBase();
            MLanguageBinding lLang = lBaseType.getLanguageBinding(Language.CPP);
            Collection<MConst> lConsts = getConsts(aIn);
            for (MConst lConst : lConsts)
            {
                if (lConst.hasValue())
                {
                    out.println(
                            aInIndent + 2,
                            "static const " + lLang.getSyntax() +
                            " CONST_" + Strings.repaceIllegal(lConst.getLID().getName().toUpperCase()) + " = " +
                            lConst.getValue().getValue() + ";");
                }
            }
        out.println(aInIndent + 1, "};");
        out.println(aInIndent, "}");
        out.println(aInIndent, "}");
        out.println(aInIndent,"#endif // " + lInclTag);
    }
}
