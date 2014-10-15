package org.opendaylight.opflex.genie.content.parse.pconfig;

import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.genie.engine.proc.Config;

/**
 * Created by midvorki on 10/8/14.
 */
public class PConfigNode
        extends ParseNode
{
    /**
     * Constructor
     */
    public PConfigNode(String aInName)
    {
        super(aInName, false);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        Config.setLibName(aInData.getNamedValue("libname", "genierated", true));
        Config.setSyntaxRelPath(aInData.getNamedValue("syntax", null, true),aInData.getNamedValue("syntaxfiletype", ".meta", true));
        Config.setLoaderRelPath(aInData.getNamedValue("loader", null, true), aInData.getNamedValue("loaderfiletype", ".cfg", true));
        Config.setGenDestPath(aInData.getNamedValue("gendest", ".", true));
        Config.setLogDirParent(aInData.getNamedValue("logfile", ".", true));

        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE,null); // TODO
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
}
