package org.opendaylight.opflex.genie.content.model.mmeta;

import java.util.Collection;
import java.util.LinkedList;
import java.util.Map;
import java.util.TreeMap;

import org.opendaylight.opflex.genie.engine.model.Cardinality;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Relator;
import org.opendaylight.opflex.genie.engine.model.RelatorCat;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNodeProp;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNodePropType;
import org.opendaylight.opflex.genie.engine.proc.Processor;
import org.opendaylight.opflex.modlan.report.Severity;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/16/14.
 *
 * parsing metadata item used in forming the parse tree for model loading
 */
public class MNode extends Item
{
    /**
     * category of this item
     */
    public static final Cat MY_CAT = Cat.getCreate("parser:meta:node");

    /**
     * Relationship category for tracking "uses" relationships. "Uses" relationship represents indirection
     * where a parsing node can rely on other node's parsing functionality, therefore, resulting parsing node
     * will delegate to the one pointed to by the "uses" relationship.
     */
    public static final RelatorCat USES_REL = RelatorCat.getCreate("parse:meta:uses", Cardinality.SINGLE);

    /**
     * Constructor
     * @param aInParent parent item that is a meta parsing node like this or null
     * @param aInLName name of the node
     */
    public MNode(Item aInParent, String aInLName)
    {
        super(MY_CAT, aInParent, aInLName);
    }

    /**
     * sets the indirection used to allow utilizing existing behavior of another parsing node for this parsing node.
     * @param aInTargetGName name of the target parsing node item
     */
    public void addUses(String aInTargetGName)
    {
        USES_REL.add(MY_CAT, getGID().getName(), MY_CAT, aInTargetGName);
    }

    /**
     * checks if this node has uses directive/relationship
     * @return true if "uses" relationship is specified for this item.
     */
    public boolean usesOtherNode()
    {
        Relator lRel = USES_REL.getRelator(getGID().getName());
        return null != lRel && lRel.hasTo();
    }

    /**
     * "uses" relator accessor
     * @return relator that represents this item's uses relationship
     */
    public Relator getUsesRelator()
    {
        return USES_REL.getRelator(getGID().getName());
    }

    /**
     * "uses" resolver. retrieves the target node item that is pointed to by "uses" relationship
     * @return
     */
    public MNode getUsesNode()
    {
        Relator lRel = getUsesRelator();
        return (MNode) (null == lRel ? null : lRel.getToItem());
    }

    /**
     * parser class accessor
     * @return parser class to be used for parsing of corresponding model node
     */
    @SuppressWarnings("rawtypes")
    private Class getParserClass()
    {
        if (null == parserClass)
        {
            if (Strings.isEmpty(parserClassName))
            {
                if (usesOtherNode())
                {
                    MNode lUsesNode = getUsesNode();
                    if (null != lUsesNode)
                    {
                        parserClass = lUsesNode.getParserClass();
                    }
                    else
                    {
                        Severity.DEATH.report(
                                toString(),
                                "parser class retrieval",
                                "no parser class defined",
                                "can't resolve indirection: " +
                                 getUsesRelator().getToRelator().getItemGName() +
                                 "; RELATOR: " + getUsesRelator() +
                                 "; TARGET RELATOR: " + getUsesRelator().getToRelator());
                    }
                }
                else
                {
                    Severity.DEATH.report(
                            toString(),
                            "parser class retrieval",
                            "no parser class defined",
                            "no explicit or indirect definition: uses other node: " +
                            usesOtherNode() +
                            "; explicit: " + parserClassName);
                }
            }
            else
            {
                try
                {
                    parserClass = Class.forName(parserClassName);
                }
                catch (Throwable lE)
                {
                    Severity.DEATH.report(
                            toString(),
                            "parser class retriever",
                            "no such parser class: " + parserClassName +
                            " uses other node: " +
                            usesOtherNode(),
                            lE);
                }
            }
        }
        return parserClass;
    }

    /**
     * adds explicit parser class name. used for manual specification of a custom parsing node,
     * @param aIn fully qualified java class name
     */
    public void addExplicitParserClassName(String aIn)
    {
        parserClassName = aIn;
    }

    public boolean hasExplicitParseClassName()
    {
        return !Strings.isEmpty(parserClassName);
    }

    /**
     * INTERNAL CALLBACK: used to initialize and register parsing nodes.
     */
    public void metaModelLoadCompleteCb()
    {
        getParseNode();
    }

    /**
     * Parse node accessor
     * @return parse node
     */
    public ParseNode getParseNode()
    {
        if (null == parseNode)
        {
            Item lParentItem = getParent();
            ParseNode lParentParseNode = null;
            if (null == lParentItem)
            {
                lParentParseNode = Processor.get().getPTree().getDocRoot();
            }
            else if (lParentItem instanceof MNode)
            {
                lParentParseNode = ((MNode) lParentItem).getParseNode();
            }
            else
            {
                Severity.DEATH.report(this.toString(), "parsing rule structure formation", "unexpected structure",
                                      "can't be parented by " + lParentItem);
            }

            @SuppressWarnings("rawtypes")
            Class lParserClass = getParserClass();
            try
            {
                ParseNode lParseNode = (ParseNode) lParserClass.getConstructor(String.class).newInstance(getLID().getName());
                LinkedList<String> lAliases = new LinkedList<String>();
                getAliases(lAliases);
                if (!lAliases.isEmpty())
                {
                    lParseNode.setAliases(lAliases);
                }
                lParentParseNode.addChild(lParseNode);
                TreeMap<String,MNodeProp> lNodeProps = new TreeMap<String,MNodeProp>();
                getProps(lNodeProps);
                for (MNodeProp lNodeProp : lNodeProps.values())
                {
                    lParseNode.addProp(
                            new ParseNodeProp(lNodeProp.getLID().getName(),lNodeProp.getType(),false));
                }
                // CHECK IF THERE'S QUALIFIER, THERE HAS TO BE NAME
                {
                    MNodeProp lQualProp = lNodeProps.get(Strings.QUAL);
                    if (null != lQualProp &&
                       !lNodeProps.containsKey(Strings.NAME))
                    {
                        lParseNode.addProp(
                                new ParseNodeProp(Strings.NAME, ParseNodePropType.QUAL,false));
                    }
                }
                // CHECK IF THERE'S NAME, THERE HAS TO BE QUALIFIER
                {
                    MNodeProp lNameProp = lNodeProps.get(Strings.NAME);
                    if (null != lNameProp &&
                        !lNodeProps.containsKey(Strings.QUAL))
                    {
                        lParseNode.addProp(
                                new ParseNodeProp(Strings.QUAL, ParseNodePropType.QUAL,false));
                    }
                }
                parseNode = lParseNode;
            }
            catch (Throwable lE)
            {
                Severity.DEATH.report(this.toString(),"parsing rule structure formation", "can't invoke constructor for: " + parserClassName, lE);
            }
            //Severity.INFO.report(toString(),"parser load", "rule loaded", "parser node: " + parseNode);
        }
        return parseNode;
    }

    public void getProps(Map<String,MNodeProp> aOut)
    {
        LinkedList<Item> lNodePropItems = new LinkedList<Item>();
        getChildItems(MNodeProp.MY_CAT,lNodePropItems);

        for (Item lIt : lNodePropItems)
        {
            if (!aOut.containsKey(lIt.getLID().getName()))
            {
                aOut.put(lIt.getLID().getName(),(MNodeProp) lIt);
            }
        }
        if (inheritProps)
        {
            MNode lUses = getUsesNode();
            if (null != lUses)
            {
                lUses.getProps(aOut);
            }
        }
    }

    public void getAliases(Collection<String> aOut)
    {
        LinkedList<Item> lAliasList = new LinkedList<Item>();
        getChildItems(MNodeAlias.MY_CAT,lAliasList);
        for (Item lIt : lAliasList)
        {
            aOut.add(lIt.getLID().getName());
        }
    }

    public void setInheritProps(boolean aIn)
    {
        inheritProps = aIn;
    }

    private ParseNode parseNode = null;
    private String parserClassName = null;
    @SuppressWarnings("rawtypes")
    private Class parserClass = null;
    private boolean inheritProps = true;
}
