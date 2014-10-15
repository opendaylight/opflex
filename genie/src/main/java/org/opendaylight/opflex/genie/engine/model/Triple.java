package org.opendaylight.opflex.genie.engine.model;

/**
 * Created by midvorki on 7/10/14.
 */
public class Triple<F,S,T>
{
    private final F first;
    private final S second;
    private final T third;

    public Triple(F aInFirst, S aInSecond, T aInThird)
    {
        this.first = aInFirst;
        this.second = aInSecond;
        this.third = aInThird;
    }

    public F getFirst()
    {
        return first;
    }

    public S getSecond()
    {
        return second;
    }

    public T getThird() { return third; }
}
