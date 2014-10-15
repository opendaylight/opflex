package org.opendaylight.opflex.genie.engine.model;

/**
 * Generalized named scalar identifier.
 *
 * Created by midvorki on 3/26/14.
 */
public class Ident implements Comparable<Ident>
{
    public Ident(
            int aInId,
            String aInName
            )
    {
        id = aInId;
        name = aInName;
    }

    /**
     * Name accessor: retrieves name of this Ident
     * @return String name
     */
    public String getName()
    {
        return name;
    }

    /**
     * Id accessor: retrieves id of this Ident
     * @return scalar id
     */
    public int getId()
    {
        return id;
    }

    /**
     * Stringifier
     */
    public String toString()
    {
        StringBuilder lSb = new StringBuilder();
        lSb.append("IDENT[");
        toIdentString(lSb);
        lSb.append(']');
        return lSb.toString();
    }


    public void toIdentString(StringBuilder aIn)
    {
        aIn.append(getName());
        /**
        aIn.append('(');
        aIn.append(getId());
        aIn.append(')');
        */
    }

    /**
     * Comparator: compares this Identifier to the one passed in.
     * @param aIn identifier to be compared to
     * @return the difference in scalar id
     */
    public int compareTo(Ident aIn)
    {
        return id - (null == aIn ? 0 : aIn.id);
    }

    private final int id;
    private final String name;
}
