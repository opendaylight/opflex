package org.opendaylight.opflex.genie.content.format.meta.mdl;

import java.util.Collection;
import java.util.LinkedList;
import java.util.TreeMap;

import org.opendaylight.opflex.genie.content.model.mmeta.MNode;
import org.opendaylight.opflex.genie.content.model.mmeta.MNodeProp;
import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.*;
import org.opendaylight.opflex.genie.engine.model.Item;

/**
 * Created by midvorki on 7/25/14.
 */
public class FMetaDef
        extends GenericFormatterTask
{
    public FMetaDef(
            FormatterCtx aInFormatterCtx, FileNameRule aInFileNameRule, Indenter aInIndenter, BlockFormatDirective aInHeaderFormatDirective, BlockFormatDirective aInCommentFormatDirective, boolean aInIsUserFile, WriteStats aInStats
                   )
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
        genNodelist(0, null, MNode.MY_CAT.getNodes().getItemsList());
    }

    private void genNodelist(int aInIndent, Item aInParent, Collection<Item> aInCol)
    {
        for (Item lItem : aInCol)
        {
            MNode lNode = (MNode) lItem;
            genNode(aInIndent,aInParent, lNode);
        }
    }
    private void genNode(int aInIndent, Item aInParent, MNode aInNode)
    {
        if (aInNode.getParent() == aInParent)
        {
            TreeMap<String, MNodeProp> lNodeProps = new TreeMap<String, MNodeProp>();
            aInNode.getProps(lNodeProps);
            LinkedList<String> lComms = new LinkedList<String>();
            lComms.add("NODE: " + aInNode.getGID().getName());
            aInNode.getComments(lComms);
            getPropComments(lNodeProps.values(), lComms);
            genComments(aInIndent, aInNode, lComms);
            out.println(aInIndent, aInNode.getLID().getName());

            if (!lNodeProps.isEmpty())
            {
                out.println(aInIndent, '[');
                genNodeProps(aInIndent, lNodeProps.values());
                out.println(aInIndent, ']');
            }

            LinkedList<Item> lChildNodes = new LinkedList<Item>();
            aInNode.getChildItems(MNode.MY_CAT, lChildNodes);

            if (!lChildNodes.isEmpty())
            {
                out.println(aInIndent, '{');
                genNodelist(aInIndent + 1, aInNode, lChildNodes);
                out.println(aInIndent, '}');
            }
        }
    }

    private void genNodeProps(int aInIndent, Collection<MNodeProp> aInNodeProps)
    {
        for (Item lItem : aInNodeProps)
        {
            MNodeProp lProp = (MNodeProp) lItem;
            genNodeProp(aInIndent + 1, lProp);
        }
    }

    private void genNodeProp(int aInIndent, MNodeProp aInProp)
    {
        switch (aInProp.getType())
        {
            case OPTION:

                out.println(aInIndent, aInProp.getLID().getName() + ";");
                break;

            default:

                out.println(aInIndent, aInProp.getLID().getName() + "=<" + aInProp.getLID().getName().toUpperCase() + ">;");
                break;
        }
    }

    public static String[] DUMMY = new String[0];
    private void genComments(int aInIndent, MNode aInNode, Collection<String> aInComments)
    {
        out.printIncodeComment(aInIndent,aInComments.toArray(DUMMY));
    }

    public void getPropComments(Collection<MNodeProp> aInNodeProps, Collection<String> aOutComments)
    {
        for (MNodeProp lThis : aInNodeProps)
        {
            aOutComments.add("- [" + lThis.getType() + "] " + lThis.getLID().getName() + ": ");
            lThis.getComments(aOutComments);
        }
    }

}
