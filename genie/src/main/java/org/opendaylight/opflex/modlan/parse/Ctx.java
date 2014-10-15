package org.opendaylight.opflex.modlan.parse;

/**
 * Created by midvorki on 3/10/14.
 */
public interface Ctx
{
    public boolean hasMore();

    public char getThis();

    public char getNext();

    public void holdThisForNext();

    public String getFileName();

    public int getCurrLineNum();
    public int getCurrColumnNum();
    public int getCurrCharNum();
}
