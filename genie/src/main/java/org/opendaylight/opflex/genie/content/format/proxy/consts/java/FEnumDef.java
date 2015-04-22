/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
package org.opendaylight.opflex.genie.content.format.proxy.consts.java;

import org.opendaylight.opflex.genie.content.model.mconst.MConst;
import org.opendaylight.opflex.genie.content.model.mprop.MProp;
import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.*;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.proc.Config;
import org.opendaylight.opflex.modlan.utils.Strings;

import java.util.Collection;

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

    public static String getClassName(Item aIn)
    {
        return (aIn instanceof MProp ? Strings.upFirstLetter(((MProp) aIn).getMClass().getLID().getName()) : "") +
               Strings.upFirstLetter(aIn.getLID().getName());
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
        String lTargetModule = getTargetModule(aInItem);
        String lOldRelativePath = aInFnr.getRelativePath();
        String lNewRelativePath = lOldRelativePath + "/src/main/java/org/opendaylight/opflex/generated/" + Strings.repaceIllegal(Config.getProjName()) + "/" + lTargetModule;

        return new FileNameRule(
                lNewRelativePath,
                null,
                aInFnr.getFilePrefix(),
                aInFnr.getFileSuffix(),
                aInFnr.getFileExtension(),
                getClassName(aInItem));
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

    public void generate()
    {
        genBody(0, getItem());
    }

    private void genBody(int aInIndent, Item aInItem)
    {
        String lTargetModule = getTargetModule(aInItem);
        String lPackageName = "org.opendaylight.opflex.generated." + Strings.repaceIllegal(Config.getProjName()) + "." + lTargetModule;
        out.println(aInIndent, "package " + lPackageName + ";");
        out.println();
        out.println(aInIndent, "public enum " + getClassName(aInItem) + " {");

        Collection<MConst> lConsts = getConsts(aInItem);
        for (MConst lConst : lConsts) {
            out.println(aInIndent + 1, Strings.repaceIllegal(lConst.getLID().getName()) + ",");
        }

        out.println(aInIndent, "}");
    }
}
