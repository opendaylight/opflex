package org.opendaylight.opflex.genie.content.model.module;

import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;

/**
 * Created by dvorkinista on 7/6/14.
 */
public class Module extends Item
{
    public static final Cat MY_CAT = Cat.getCreate("module");

    public static Module get(String aInName)
    {
        return get(aInName, false);
    }
    public static synchronized  Module get(String aInName, boolean aInCreateIfNotFound)
    {
        Module lModule = (Module) MY_CAT.getItem(aInName);
        if (null == lModule && aInCreateIfNotFound)
        {

            if (null == MY_CAT.getItem(aInName))
            {
                lModule = new Module(aInName);
            }
        }
        return lModule;
    }

    private Module(String aInLName)
    {
        super(MY_CAT, null, aInLName);
    }

    public static Module getModule(Item aIn)
    {
        return (Module) aIn.getAncestorOfCat(Module.MY_CAT);
    }
}
