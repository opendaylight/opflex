package org.opendaylight.opflex.genie.content.parse.pformatter;

import org.opendaylight.opflex.genie.content.model.mformatter.MFormatterFeature;
import org.opendaylight.opflex.genie.content.model.mformatter.MFormatterTask;
import org.opendaylight.opflex.genie.engine.format.FormatterTaskType;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.genie.engine.parse.model.ParseNode;
import org.opendaylight.opflex.genie.engine.parse.modlan.Node;
import org.opendaylight.opflex.genie.engine.parse.modlan.ParseDirective;
import org.opendaylight.opflex.genie.engine.proc.Config;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/24/14.
 */
public class PTaskNode extends ParseNode
{
    public PTaskNode(String aInName)
    {
        super(aInName);
    }

    public Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItem)
    {

        MFormatterTask lTask = new MFormatterTask(
                    (MFormatterFeature) aInParentItem,
                    aInData.getNamedValue(Strings.NAME,null, true));

        lTask.setTarget(aInData.getNamedValue(Strings.TARGET, null, true));
        lTask.setTargetCategory(
                aInData.getNamedValue(Strings.CATEGORY, null, FormatterTaskType.GENERIC != lTask.getTarget()));
        String lRelPath = aInData.getNamedValue("relative-path", null, false);
        lTask.setRelativePath(Config.getLibName() + (Strings.isAny(lRelPath) ? "" : "/" + lRelPath));
        lTask.setFileType(aInData.getNamedValue("file-type", null, true));
        lTask.setFilePrefix(aInData.getNamedValue("file-prefix", null, false));
        lTask.setFileSuffix(aInData.getNamedValue("file-suffix", null, false));
        lTask.setFormatterClass(aInData.getNamedValue("formatter", null, true));
        lTask.setIsUser(aInData.checkFlag("user"));
        return new Pair<ParseDirective, Item>(ParseDirective.CONTINUE,lTask);
    }
}
