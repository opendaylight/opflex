package org.opendaylight.opflex.genie.content.parse.pmeta;

import java.io.StringWriter;

import org.opendaylight.opflex.genie.content.model.mmeta.MNode;
import org.opendaylight.opflex.genie.content.model.mmeta.NodeType;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/16/14.
 *
 * This is a parsing node for meta abstraction for representing loadable parsing nodes.
 * Parsing nodes are registered in corresponding STRUCT.meta file that represents the
 * meta-structure of the parsed tree.
 */
public class PNode
        extends ParseNode
{
    /**
     * Constructor
     */
    public PNode(NodeType aInType)
    {
        super(aInType.getName(), true);
        type = aInType;
    }

    /**
     * checks if the property is supported by this node. this overrides behavior to always return true
     * @param aInName name of the property
     * @return always returns true
     */
    public boolean hasProp(String aInName)
    {
        return true;
    }

    /**
     * Parsing callback to indicate beginning of the node parsing. This callback creates a parsing metadata item that is
     * later used in forming the parse tree for model loading.
     *
     * @param aInData parsed data node
     * @param aInParentItem parent item that was a result of parsing parent nodes. can be null if no parent.
     * @return a pair of resulting parse directive and the item that was produced as result of parsing this data node.
     */
    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        // TRY TO FIND EXISTING META NODE
        MNode lMNode = (MNode) (null != aInParentItem ?
                                    aInParentItem.getChildItem(MNode.MY_CAT, aInData.getQual()) :
                                    MNode.MY_CAT.getItem(aInData.getQual()));


        // CHECK IF FOUND
        if (null == lMNode)
        {
            // NOT FOUND: CREATE META NODE THAT CORRESPONDS TO THIS NODE PARSING RULE
            lMNode = new MNode(aInParentItem, aInData.getQual());
        }

        if (NodeType.NODE == type)
        {
            // CHECK IF IT HAS INDIRECTION ALREADY SPECIFIED
            if (!lMNode.usesOtherNode())
            {
                // CHECK IF WE HAVE TO USE OTHER NODE'S PARSER
                String lUse = aInData.getNamedValue("use", null, false);
                if (!Strings.isEmpty(lUse))
                {
                    lMNode.addUses(lUse);
                }
            }

            // CHECK IF NEITHER INDIRECTION OR EXPLICIT CLASS ARE SPECIFIED
            if (!(lMNode.usesOtherNode() || lMNode.hasExplicitParseClassName()))
            {
                // NEITHER INDIRECTION OR EXPLICIT CLASS ARE SPECIFIED

                // GET THE PARSE NODE'S EXPLICIT PARSING CLASS DIRECTIVE
                String lExplicit = aInData.getNamedValue("explicit", null, false);

                // CHECK IF DIRECT PARSING CLASS DIRECTIVE IS SET
                String lParserImplClassName = null;
                if (!Strings.isEmpty(lExplicit))
                {
                    lParserImplClassName = lExplicit;
                }
                // CHECK IF THIS THIS IS A GENERIC/PLACEHOLDER NODE
                else if (aInData.checkFlag("generic"))
                {
                    lParserImplClassName = "org.opendaylight.opflex.genie.engine.parse.model.ParseNode";
                }
                // THIS IS AN AUTO-PICKED PARSING NODE... LET'S FABRICATE THE NAME
                else
                {
                    // DETERMINE DEFAULT PARSER NAME FOR THIS NODE
                    String lQual = aInData.getQual();
                    String lNs = aInData.getNamedValue("namespace", lQual, false);
                    StringWriter lParserImplClassNameBuff = new StringWriter();
                    lParserImplClassNameBuff.append("org.opendaylight.opflex.genie.content.parse.p");
                    lParserImplClassNameBuff.append(lNs.toLowerCase());
                    lParserImplClassNameBuff.append(".P");
                    lParserImplClassNameBuff.append(Strings.upFirstLetter(lQual));
                    lParserImplClassNameBuff.append("Node");
                    lParserImplClassName = lParserImplClassNameBuff.toString();
                }
                // Set the parser node class name
                lMNode.addExplicitParserClassName(lParserImplClassName);
            }
            lMNode.setInheritProps(aInData.getNamedOption("inherit-props",true));
        }
        return new Pair<ParseDirective, Item>(
                ParseDirective.CONTINUE,
                lMNode);
    }

    private final NodeType type;
}
