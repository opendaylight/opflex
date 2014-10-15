package org.opendaylight.opflex.genie.content.format.meta.mdl;

import java.util.TreeMap;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.mprop.MProp;
import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.*;
import org.opendaylight.opflex.genie.engine.model.Ident;

/**
 * Created by midvorki on 8/4/14.
 */
public class FContDef
        extends GenericFormatterTask
{
    public FContDef(
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

    public void generate()
    {
        out.println();
        MClass lRoot = MClass.getContainmentRoot();
        genClass(0, lRoot);
    }

    private void genClass(int aInIndent, MClass aInClass)
    {
        out.print(aInIndent, aInClass.getFullConcatenatedName());
        genProps(aInIndent + 1, aInClass);
        genContained(aInIndent, aInClass);
    }

    private void genProps(int aInIndent, MClass aInClass)
    {
        TreeMap<String,MProp> lProps = new TreeMap<String, MProp>();
        aInClass.findProp(lProps, false);
        if (!lProps.isEmpty())
        {
            out.print('[');
            for (MProp lProp : lProps.values())
            {
                genProp(aInIndent,lProp);
            }
            out.println(']');
        }
        else
        {
            out.println();
        }
    }

    private void genProp(int aInIndent, MProp aInProp)
    {
        out.println();
        out.print(aInIndent,
                  aInProp.getLID().getName() +
                  "=<" +
                  aInProp.getType(false).getFullConcatenatedName() + "/" + aInProp.getType(true).getFullConcatenatedName() + "|group:" + aInProp.getGroup() + ">;");
    }

    private void genContained(int aInIndent, MClass aInParent)
    {
        TreeMap<Ident,MClass> lContained = new TreeMap<Ident,MClass>();
        aInParent.getContainsClasses(lContained, true, true);
        if (!lContained.isEmpty())
        {
            out.println(aInIndent, '{');
            for (MClass lClass : lContained.values())
            {
                genClass(aInIndent+1, lClass);
            }
            out.println(aInIndent, '}');
        }
    }
}