/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
package org.opendaylight.opflex.genie.content.format.proxy.meta.java;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.mconst.ConstAction;
import org.opendaylight.opflex.genie.content.model.mconst.MConst;
import org.opendaylight.opflex.genie.content.model.mnaming.MNameComponent;
import org.opendaylight.opflex.genie.content.model.mnaming.MNameRule;
import org.opendaylight.opflex.genie.content.model.mownership.MOwner;
import org.opendaylight.opflex.genie.content.model.mprop.MProp;
import org.opendaylight.opflex.genie.content.model.mtype.MType;
import org.opendaylight.opflex.genie.content.model.mtype.MTypeHint;
import org.opendaylight.opflex.genie.content.model.mtype.TypeInfo;
import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.*;
import org.opendaylight.opflex.genie.engine.model.Ident;
import org.opendaylight.opflex.genie.engine.proc.Config;
import org.opendaylight.opflex.modlan.utils.Strings;

import java.util.Collection;
import java.util.Map;
import java.util.TreeMap;

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
        String lNewRelativePath = lOldRelativePath + "/src/main/java/org/opendaylight/opflex/generated/" +
                                  Strings.repaceIllegal(Config.getProjName()) + "/metadata";

        return new FileNameRule(
                lNewRelativePath,
                null,
                aInFnr.getFilePrefix(),
                aInFnr.getFileSuffix(),
                aInFnr.getFileExtension(),
                "Metadata");
    }

    public void generate()
    {
        out.println(0, "package org.opendaylight.opflex.generated." + Strings.repaceIllegal(Config.getProjName()) + ".metadata;");
        out.println();
        out.println(0, "import org.opendaylight.opflex.modb.*;");
        out.println(0, "import java.util.*;");
        out.println();
        out.println(0, "public class Metadata {");
        out.println();
        generateMetaAccessor(1);
        out.println(0, "}");
    }

    private void generateMetaAccessor(int aInIndent)
    {
        out.println(aInIndent, "// statically initialize model metadata");
        out.println(aInIndent, "public static void init() {");
        out.println(aInIndent + 1, "ModelMetadata.addMetadata(\"" + Config.getProjName() + "\", ");
        generateClassDefs(aInIndent + 2);
        out.println(aInIndent + 2, ");");
        out.println(aInIndent, "}");
    }

    private void generateClassDefs(int aInIndent)
    {
        boolean lFirst = true;
        for (MClass lClass : MClass.getConcreteClasses())
        {
            if (lClass.isConcrete())
            {
                lFirst = checkIfFirst(aInIndent, lFirst);
                genMo(aInIndent, lClass);
            }
        }
    }

    public static String getClassType(MClass aIn)
    {
        if (isPolicy(aIn))
        {
            return "ClassInfo.ClassType.POLICY";
        }
        else if (isObservable(aIn))
        {
            return "ClassInfo.ClassType.OBSERVABLE";
        }
        else if (isRelationshipSource(aIn))
        {
            return "ClassInfo.ClassType.RELATIONSHIP";
        }
        else if (isRelationshipTarget(aIn))
        {
            return "ClassInfo.ClassType.REVERSE_RELATIONSHIP";
        }
        else  if (isRelationshipResolver(aIn))
        {
            return "ClassInfo.ClassType.RESOLVER";
        }
        else if (!aIn.isConcrete())
        {
            return "ClassInfo.ClassType.ABSTRACT";
        }
        else if (isGlobalEp(aIn))
        {
            return "ClassInfo.ClassType.LOCAL_ENDPOINT";
        }
        else if (isLocalEp(aIn))
        {
            return "ClassInfo.ClassType.LOCAL_ONLY";
        }
        else
        {
            return "ClassInfo.ClassType.LOCAL_ONLY";
        }
    }
    
    public static String getTypeName(MType aIn)
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

    public static boolean isLocalEp(MClass aIn)
    {
        return aIn.isSubclassOf("epr/LocalEp");

    }

    public static boolean isGlobalEp(MClass aIn)
    {
        return aIn.isSubclassOf("epr/ReportedEp");
    }

    public static boolean isPolicy(MClass aIn)
    {
        return aIn.isSubclassOf("policy/Component") || aIn.isSubclassOf("policy/Definition");
    }

    public static boolean isObservable(MClass aIn)
    {
        return aIn.isSubclassOf("observer/Observable");
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
        out.print(aInIndent, "new ClassInfo(" + aInClass.getGID().getId() + ", ");
        out.println(getClassType(aInClass) + ", \"" + aInClass.getFullConcatenatedName() + "\", \"" + getOwner(aInClass) + "\",");
        genProps(aInIndent + 1, aInClass);
        genNamingProps(aInIndent + 1, aInClass);
        out.println(aInIndent + 1, ")");
    }

    private String toUnsignedStr(int aInInt) {
    	return Long.toString(aInInt & 0xFFFFFFFFL) + "L";
    }

    private boolean checkIfFirst(int aInIndent, boolean aInIsFirst)
    {
        if (!aInIsFirst)
        {
            out.println(aInIndent + 1, ",");
        }
        return false;
    }
    
    private void genProps(int aInIndent, MClass aInClass)
    {
        Map<String,MProp> lProps = new TreeMap<>();
        aInClass.findProp(lProps,true);

        Map<Ident,MClass> lConts = new TreeMap<>();
        aInClass.getContainsClasses(lConts, true, true);

        if (lProps.size() + lConts.size() == 0)
        {
            out.println(aInIndent, "Collections.emptyList()");
        }
        else
        {
            out.println(aInIndent, "Arrays.asList(");
            boolean lFirst = true;
            // HANDLE PROPS
            for (MProp lProp : lProps.values())
            {
                MType lPropType = lProp.getType(false);
                MType lPrimitiveType = lPropType.getBuiltInType();
                MTypeHint lHint = lPrimitiveType.getTypeHint();

                int lLocalId = lProp.getPropId(aInClass);

                if (isRelationshipSource(aInClass))
                {
                    if (lProp.getLID().getName().equalsIgnoreCase("targetName"))
                    {
                        lFirst = checkIfFirst(aInIndent + 1, lFirst);
                        out.println(aInIndent + 1,
                                "new PropertyInfo(" + toUnsignedStr(lLocalId) + ", \"target\", PropertyInfo.Type.REFERENCE, PropertyInfo.Cardinality.SCALAR) // " + lProp.toString());
                    }
                }
                else if (lProp.getLID().getName().equalsIgnoreCase("source") && isRelationshipTarget(aInClass))
                {
                    lFirst = checkIfFirst(aInIndent + 1, lFirst);
                    out.println(aInIndent + 1,
                            "new PropertyInfo(" + toUnsignedStr(lLocalId) + ", \"source\", PropertyInfo.Type.REFERENCE, PropertyInfo.Cardinality.SCALAR) // " + lProp.toString());
                }
                else if (Config.isEnumSupport() &&
                        (lHint.getInfo() == TypeInfo.ENUM ||
                                lHint.getInfo() == TypeInfo.BITMASK))
                {
                    lFirst = checkIfFirst(aInIndent + 1, lFirst);
                    out.println(
                            aInIndent + 1,
                            "new PropertyInfo(" + toUnsignedStr(lLocalId) + ", \"" + lProp.getLID().getName() + "\", PropertyInfo.Type." + getTypeName(lPrimitiveType) + ", PropertyInfo.Cardinality.SCALAR,");
                    genConsts(aInIndent + 2, lProp, lPropType);
                    out.println(") // " + lProp.toString());
                }
                else
                {
                    lFirst = checkIfFirst(aInIndent + 1, lFirst);
                    out.println(
                            aInIndent + 1,
                            "new PropertyInfo(" + toUnsignedStr(lLocalId) + ", \"" + lProp.getLID().getName() + "\", PropertyInfo.Type." + getTypeName(lPrimitiveType) + ", PropertyInfo.Cardinality.SCALAR) // "
                                    + lProp.toString());
                }
            }


            // HANDLE CONTAINED CLASSES
            for (MClass lContained : lConts.values())
            {
                if (lFirst)
                {
                    lFirst = false;
                }
                else
                {
                    out.println(aInIndent + 1, ",");
                }
                out.println(aInIndent + 1, "new PropertyInfo(" + toUnsignedStr(lContained.getClassAsPropId(aInClass)) + ", \"" + lContained.getFullConcatenatedName() + "\", PropertyInfo.Type.COMPOSITE, " + lContained.getGID().getId() + ", PropertyInfo.Cardinality.VECTOR) // " + lContained.toString());
            }

            out.println(aInIndent + 1, "),");
        }
    }
    private void genConsts(int aInIndent, MProp aInProp, MType aInType)
    {
        Map<String, MConst> lConsts = new TreeMap<>();
        aInProp.findConst(lConsts, true);
        out.println(aInIndent, "new EnumInfo(\"" + aInType.getFullConcatenatedName() + "\",");

        if (lConsts.isEmpty())
        {
            out.println(aInIndent, "Collections.emptyList()");
        }
        else
        {
            boolean lFirst = true;
            for (MConst lConst : lConsts.values())
            {
                if (ConstAction.REMOVE != lConst.getAction())
                {
                    lFirst = checkIfFirst(aInIndent + 1, lFirst);
                    out.println(aInIndent + 1, "new ConstInfo(\"" + lConst.getLID().getName() + "\", " + lConst.getValue().getValue() + ") // " + lConst);
                }
            }
            if (lFirst)
            {
                out.println(aInIndent, "Collections.emptyList()) // NO CONSTS DEFINED");
            }
        }
        out.print(aInIndent + 1,")");
    }

    private void genNamingProps(int aInIndent, MClass aInClass)
    {
        MNameRule lNr = getNamingRule(aInClass);

        if (null == lNr)
        {
            out.println(aInIndent, "Collections.<Long>emptyList() // no naming rule; assume cardinality of 1 in any containment rule");
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
                out.println(aInIndent, "Collections.<Long>emptyList() // no naming props in rule " + lNr + "; assume cardinality of 1");
            }
            else
            {
                out.println(aInIndent, "Arrays.asList( // " + lNr);
                boolean lFirst = true;
                for (MNameComponent lIt : lComps)
                {
                    if (lIt.hasPropName())
                    {
                        MProp lProp = aInClass.findProp(lIt.getPropName(),false);
                        if (null != lProp)
                        {
                            lFirst = checkIfFirst(aInIndent + 1, lFirst);
                            out.println(aInIndent + 1, lProp.getPropId(aInClass) + "L //" + lProp + " of name component " + lIt);
                        }
                    }
                }
                if (!lFirst)
                {
                    out.print(aInIndent + 1, ")");
                }
            }
        }
    }
}
