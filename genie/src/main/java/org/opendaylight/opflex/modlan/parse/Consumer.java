package org.opendaylight.opflex.modlan.parse;

/**
 * Created by midvorki on 3/10/14.
 */
public interface Consumer
{
    Data onDocBegin(String aInName);
    Data onDocEnd(String aInName);

    Data onNodeBegin(String aInName);
    Data onNodeEnd(String aInName);

    Data onQual(String aIn);
    Data onComment(String aInLine);
    Data onText(String aInLine);
    Data onValue(String aInValue);

    Data onContentBegin(String aInName);
    Data onContentEnd(String aInName);

    void setEngine(Engine aIn);
}
