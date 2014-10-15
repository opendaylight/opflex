package org.opendaylight.opflex.genie.content.model.mownership;

import java.util.LinkedList;
import java.util.Map;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.module.Module;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 9/27/14.
 */
public class MClassRule extends MOwnershipRule
{
    public static final Cat MY_CAT = Cat.getCreate("mowner:class");

    public MClassRule(MOwner aInParent, String aInNameOrAll)
    {
        super(MY_CAT,aInParent,aInNameOrAll,DefinitionScope.OWNER);
    }

    public MClassRule(MModuleRule aInParent, String aInNameOrAll)
    {
        super(MY_CAT,aInParent,aInNameOrAll,DefinitionScope.MODULE);
    }

    public void getClasses(Map<String, MClass> aOut)
    {
        switch (getDefinitionScope())
        {
            case OWNER:

                {
                    if (isWildCard())
                    {
                        // THIS IS WILD CARD, ALL CLASSES ARE IN SCOPE
                        for (Item lIt : MClass.MY_CAT.getNodes().getItemsList())
                        {
                            aOut.put(lIt.getGID().getName(),(MClass) lIt);
                        }
                    }
                    else
                    {
                        // NOT A WILD CARD, FIND SPECIFIC CLASS
                        MClass lIt = MClass.get(getLID().getName());
                        if (null != lIt)
                        {
                            aOut.put(lIt.getGID().getName(), lIt);
                        }
                        else
                        {
                            Severity.DEATH.report("ownership rule class retrieval", "", "",
                                                  "no such class: " + getLID().getName());
                        }
                    }
                }
                break;

            case MODULE:

                {
                    MModuleRule lModuleRule = (MModuleRule) getParent();
                    if (lModuleRule.isWildCard())
                    {
                        // MODULE IS WILDCARD --> ANY MODULE, MATCH LOCAL NAMES OF THE CLASSES
                        if (isWildCard())
                        {
                            // ALL CLASSES IN THE UNIVERSE
                            for (Item lIt : MClass.MY_CAT.getNodes().getItemsList())
                            {
                                aOut.put(lIt.getGID().getName(),(MClass) lIt);
                            }
                        }
                        else
                        {
                            // MATCH LOCAL NAMES OF THE CLASSES IN THE UNIVERSE
                            for (Item lIt : MClass.MY_CAT.getNodes().getItemsList())
                            {
                                if (lIt.getLID().getName().equals(getLID().getName()))
                                {
                                    aOut.put(lIt.getGID().getName(), (MClass) lIt);
                                }
                            }
                        }
                    }
                    else
                    {
                        // MODULE IS SPECIFIED
                        Module lModule = Module.get(lModuleRule.getLID().getName());
                        if (null != lModule)
                        {
                            LinkedList<Item> lIts = new LinkedList<Item>();
                            lModule.getChildItems(MClass.MY_CAT, lIts);
                            for (Item lIt : lIts)
                            {
                                aOut.put(lIt.getGID().getName(),(MClass) lIt);
                            }
                        }
                        else
                        {
                            Severity.DEATH.report("ownership rule class retrieval", "", "",
                                                  "no such module: " + lModuleRule.getLID().getName());
                        }
                    }
                }
        }
    }
}
