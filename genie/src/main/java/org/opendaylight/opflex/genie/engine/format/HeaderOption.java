package org.opendaylight.opflex.genie.engine.format;

/**
 * Created by midvorki on 11/5/14.
 */
public enum HeaderOption
{
    GENIE_VAR_FULL_FILE_NAME("GENIE_VAR_FULL_FILE_NAME"),
    ;
    private HeaderOption(String aInName)
    {
        name = aInName;
    }

    public String getName() { return name; }

    private final String name;
}
