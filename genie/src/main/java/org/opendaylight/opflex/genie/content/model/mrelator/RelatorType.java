package org.opendaylight.opflex.genie.content.model.mrelator;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 8/5/14.
 */
public enum RelatorType
{
    DIRECT_ASSOCIATION(new String[]{"direct-association", "relation"} , false, true, true),
    NAMED_ASSOCIATION(new String[]{"named-association"}, true, true, true),
    DIRECT_DEPENDENCY(new String[]{"direct-dependency"}, false, true, true),
    NAMED_DEPENDENCY(new String[]{"named-dependency", "dependency"}, true, true, true),
    REFERENCE(new String[]{"reference"}, false, true, false);

    public static RelatorType get(String aIn)
    {
        for (RelatorType lThis : RelatorType.values())
        {

            if (lThis.toString().equalsIgnoreCase(aIn))
            {
                return lThis;
            }
            for (String lName : lThis.names)
            {
                if (lName.equalsIgnoreCase(aIn))
                {
                    return lThis;
                }
            }
        }
        Severity.DEATH.report("relator-type", "get", "", "no relator type: " + aIn);

        return NAMED_DEPENDENCY;
    }
    RelatorType(
        String[] aInNames,
        boolean aInIsNamed,
        boolean aInHasSourceObject,
        boolean aInHasTargetObject)
    {
        names = aInNames;
        isNamed = aInIsNamed;
        hasSourceObject = aInHasSourceObject;
        hasTargetObject = aInHasTargetObject;
    }

    public boolean isNamed() { return isNamed; }

    public boolean hasSourceObject() { return hasSourceObject; }
    public boolean hasTargetObject() { return hasTargetObject; }

    private final String[] names;
    private final boolean hasSourceObject;
    private final boolean hasTargetObject;
    private final boolean isNamed;
}
