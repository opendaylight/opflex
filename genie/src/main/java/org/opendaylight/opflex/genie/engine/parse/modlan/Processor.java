package org.opendaylight.opflex.genie.engine.parse.modlan;

import java.util.Collection;

import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;

/**
 * Created by midvorki on 3/22/14.
 */
public interface Processor
{
    Pair<ParseDirective,Item> beginCB(Node aInData, Item aInParentItemOrNull);
    void endCB(Node aInData, Item aInItemOrNull);
    Processor getChild(String aInName);
    boolean hasProp(String aInName);
    Collection<String> getPropNames();
}
