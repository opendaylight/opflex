package org.opendaylight.opflex.genie.content.model.mrelator;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 8/5/14.
 */
public enum RelatorType
{
    DIRECT_ASSOCIATION(new String[]{"direct-association", "relation"} , false, true, true, false),
    NAMED_ASSOCIATION(new String[]{"named-association"}, true, true, true, false),
    DIRECT_DEPENDENCY(new String[]{"direct-dependency"}, false, true, true, true),
    NAMED_DEPENDENCY(new String[]{"named-dependency", "dependency"}, true, true, true, true),
    REFERENCE(new String[]{"reference"}, false, true, false, false);

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
    private RelatorType(
            String[] aInNames,
            boolean aInIsNamed,
            boolean aInHasSourceObject,
            boolean aInHasTargetObject,
            boolean aInHasResolverObject)
    {
        names = aInNames;
        isNamed = aInIsNamed;
        hasSourceObject = aInHasSourceObject;
        hasTargetObject = aInHasTargetObject;
        hasResolverObject = aInHasResolverObject;
    }

    public boolean isNamed() { return isNamed; }
    public boolean isDirect() { return !isNamed; }

    public boolean hasSourceObject() { return hasSourceObject; }
    public boolean hasTargetObject() { return hasTargetObject; }
    public boolean hasResolverObject() { return hasResolverObject; }

    private final String[] names;
    private final boolean hasSourceObject;
    private final boolean hasTargetObject;
    private final boolean hasResolverObject;
    private final boolean isNamed;
}
