package org.opendaylight.opflex.genie.engine.model;

/**
 * Created by midvorki on 7/8/14.
 */
public class Pair<F, S>
{
    private final F first;
    private final S second;

    public Pair(F aInFirst, S aInSecond)
    {
        this.first = aInFirst;
        this.second = aInSecond;
    }

    public F getFirst()
    {
        return first;
    }

    public S getSecond()
    {
        return second;
    }

    public String toString()
    {
        return "PAIR[" + first +"," + second + "]";
    }
}