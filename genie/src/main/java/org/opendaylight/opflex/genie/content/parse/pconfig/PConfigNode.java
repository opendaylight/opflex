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
    public PConfigNode()
    {
        super("config", false);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {
        Config.setLibName(aInData.getNamedValue("libname", "generated", true));
        Config.setLibVersion(aInData.getNamedValue("libversion", "1.0", true));
        Config.setLibtoolVersion(aInData.getNamedValue("libtoolversion", "0:0:0", true));
        Config.setSyntaxRelPath(aInData.getNamedValue("syntax", null, true),aInData.getNamedValue("syntaxfiletype", ".meta", true));
        Config.setLoaderRelPath(aInData.getNamedValue("loader", null, true), aInData.getNamedValue("loaderfiletype", ".cfg", true));
        Config.setFormatterRelPath(aInData.getNamedValue("formatter", null, true), aInData.getNamedValue("formatterfiletype", ".cfg", true));
        Config.setGenDestPath(aInData.getNamedValue("gendest", ".", true));
        
        Config.setLogDirParent(aInData.getNamedValue("logfile", ".", true));
        Config.setIsEnumSupport(
                    aInData.validateBoolean(
                            "enums",
                            aInData.getNamedValue("enums", null, false),
                            true));

        Config.setHeaderFormat(aInData.getNamedValue("headerpath", null, false));

        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE,null);
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
