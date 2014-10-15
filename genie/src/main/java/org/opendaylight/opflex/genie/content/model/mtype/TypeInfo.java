package org.opendaylight.opflex.genie.content.model.mtype;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 9/24/14.
 */
public enum TypeInfo
{
    SCALAR("scalar"),
    ENUM("enum"),
    BITMASK("bitmask"),
    ASCII_CHAR("ascii:char"),
    ASCII_STRING("ascii:string"),
    REFERENCE_URI("reference:uri"),
    REFERENCE_LRI("reference:lri"),
    REFERENCE_URL("reference:url"),
    REFERENCE_NAMED("reference:named"),
    REFERENCE_CLASS("reference:class"),
    REFERENCE_UUID("reference:uuid"),
    ADDRESS_IP_ANY("address:ip"),
    ADDRESS_IPv4("address:ipv4"),
    ADDRESS_IPv6("address:ipv6"),
    ADDRESS_MAC("address:mac"),
    ;
    public static TypeInfo get(String aIn)
    {

        for (TypeInfo lThis : TypeInfo.values())
        {
            if (lThis.tag.equalsIgnoreCase(aIn))
            {
                return lThis;
            }
        }
        Severity.DEATH.report(
                "type:info", "retrieval of type info", "", "no such type info directive: " + aIn);

        return null;
    }

    private TypeInfo(String aInTag)
    {
        tag = aInTag;
    }

    private final String tag;
}
