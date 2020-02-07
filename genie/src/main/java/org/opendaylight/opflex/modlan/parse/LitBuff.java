package org.opendaylight.opflex.modlan.parse;

/**
 * Created by midvorki on 3/16/14.
 */
public class LitBuff
{
    public LitBuff()
    {
        reset();
    }

    public void append(char aIn)
    {
        sb.append(aIn);
    }

    public void reset()
    {
        if (null == sb || 0 < sb.length())
        {
            sb = new StringBuilder(); //new StringBuffer();
        }
    }

    public int getSize()
    {
        return null == sb ? 0 : sb.length();
    }

    public String toString()
    {
        return sb.toString();
    }

    private StringBuilder sb = null;
}
