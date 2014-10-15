package org.opendaylight.opflex.modlan.parse;

/**
 * Created by midvorki on 3/10/14.
 */
public interface Consumer
{
    public Data onDocBegin(String aInName);
    public Data onDocEnd(String aInName);

    public Data onNodeBegin(String aInName);
    public Data onNodeEnd(String aInName);

    public Data onQual(String aIn);
    public Data onComment(String aInLine);
    public Data onText(String aInLine);
    public Data onValue(String aInValue);

    public Data onContentBegin(String aInName);
    public Data onContentEnd(String aInName);

    public void setEngine(Engine aIn);
}
