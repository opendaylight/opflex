package org.opendaylight.opflex.genie.engine.parse.model;

/**
 * Created by midvorki on 7/22/14.
 */
public class ParseNodeProp
{
    public ParseNodeProp(
            String aInName,
            ParseNodePropType aInType,
            boolean aInIsMandatory)
    {
        name = aInName;
        type = aInType;
        isMandatory = aInIsMandatory;
    }

    public String getName()
    {
        return name;
    }

    public ParseNodePropType getType()
    {
        return type;
    }

    public boolean isMandatory()
    {
        return isMandatory;
    }

    public String toString()
    {
        return type + "<" + name + "|" + (isMandatory ? "mandatory" : "optional") + ">";
    }

    private final String name;
    private final ParseNodePropType type;
    private final boolean isMandatory;
}
