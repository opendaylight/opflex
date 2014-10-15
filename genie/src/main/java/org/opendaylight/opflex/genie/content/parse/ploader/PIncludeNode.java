package org.opendaylight.opflex.genie.content.parse.ploader;

import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.load.*;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/25/14.
 */
public class PIncludeNode extends ParseNode
{
    public PIncludeNode(String aInName)
    {
        super(aInName);
    }

    public Pair<ParseDirective, Item> beginCB(Node aInData, Item aInParentItem)
    {
        Node lFeature = aInData.getParent();
        Node lDomain = lFeature.getParent();
        LoaderDomainMeta lDomainMeta =
                LoaderRegistry.get().getDomain(lDomain.getNamedValue(Strings.NAME, null, true), true);
        LoaderFeatureMeta lFeatureMeta = lDomainMeta.getFeature(lFeature.getNamedValue(Strings.NAME, null, true), true);
        String lDir = aInData.getNamedValue("dir", null, true);
        String lExt = aInData.getNamedValue("ext", null, true);
        if (!lExt.startsWith("."))
        {
            lExt = '.' + lExt;
        }
        String lName = aInData.getNamedValue(Strings.NAME,(lDir + lExt),true);
        LoadStage lStage = LoadStage.get(aInData.getNamedValue("stage", "load", true));
        LoaderIncludeMeta lIncl = new LoaderIncludeMeta(lName, lDir,lExt, lStage);
        lFeatureMeta.addInclude(lIncl);
        //System.out.println("==========> ADDED: " + lIncl);
        return null;
    }
}
