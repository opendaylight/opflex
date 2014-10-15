package org.opendaylight.opflex.genie.content.parse.modlan;

import org.opendaylight.opflex.genie.content.model.mmeta.NodeType;
import org.opendaylight.opflex.genie.content.parse.pconfig.PConfigNode;
import org.opendaylight.opflex.genie.content.parse.pmeta.PNode;
import org.opendaylight.opflex.genie.content.parse.pmeta.PProp;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.model.ProcessorNode;
import org.opendaylight.opflex.genie.engine.parse.model.ProcessorTree;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 3/22/14.
 */
public class ParseRegistry
{
    public static ProcessorTree init()
    {
        ProcessorTree lPTree = new ProcessorTree();
        {
            ProcessorNode lDocRoot = new ParseNode(ProcessorTree.DOC_ROOT_NAME);
            lPTree.addChild(lDocRoot);
            {
                ProcessorNode metadata = new ParseNode(Strings.METADATA);
                lDocRoot.addChild(metadata);

                for (NodeType lNt : NodeType.values())
                {
                    {
                        ProcessorNode node = new PNode(lNt);
                        metadata.addChild(node);
                        {
                            PProp prop = new PProp(Strings.PROP);
                            node.addChild(prop);
                        }
                        {
                            PProp qual = new PProp(Strings.QUAL);
                            node.addChild(qual);
                        }
                        {
                            PProp option = new PProp(Strings.OPTION);
                            node.addChild(option);
                        }
                        if (NodeType.REFERENCE == lNt)
                        {
                            ProcessorNode subNode = new PNode(NodeType.NODE);
                            node.addChild(subNode);
                            {
                                PProp prop = new PProp(Strings.PROP);
                                subNode.addChild(prop);
                            }
                            {
                                PProp qual = new PProp(Strings.QUAL);
                                subNode.addChild(qual);
                            }
                            {
                                PProp option = new PProp(Strings.OPTION);
                                subNode.addChild(option);
                            }
                        }
                    }
                }
            }
            {
                ProcessorNode config = new PConfigNode("config");
                lDocRoot.addChild(config);
            }
        }
        return lPTree;
    }
}
