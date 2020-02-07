package org.opendaylight.opflex.modlan.parse;

/**
 * Created by midvorki on 3/10/14.
 */
public interface Ctx
{
    boolean hasMore();

    char getThis();

    char getNext();

    void holdThisForNext();

    String getFileName();

    int getCurrLineNum();
    int getCurrColumnNum();
    int getCurrCharNum();
}
