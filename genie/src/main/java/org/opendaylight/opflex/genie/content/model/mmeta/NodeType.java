package org.opendaylight.opflex.genie.content.model.mmeta;

/**
 * Created by midvorki on 7/22/14.
 */
public enum NodeType
{
    NODE("node"),
    REFERENCE("ref")
    ;

    NodeType(String aInModelName)
    {
        modelName = aInModelName;
    }

    public String getName()
    {
        return modelName;
    }
    /**
     * stringifier
     */
    public String toString()
    {
        return modelName;
    }

    private final String modelName;
}
