package org.opendaylight.opflex.modlan.parse;

/**
 * Created by midvorki on 3/16/14.
 */
public class LitBuff
{
    public static final int BUFF_MAX_SIZE = 512;

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

    /**
     *
     * public static final int BUFF_MAX_SIZE = 512;

     public LitBuff()
     {
     reset();
     }

     public void append(char aIn)
     {
     literal[literalIdx++] = aIn;
     literal[literalIdx] = '\0';
     }

     public void reset()
     {
     literalIdx = 0;
     literal[0] = '\0';
     }

     public int getSize()
     {
     return literalIdx;
     }

     public String toString()
     {
     return new String(literal);
     }

     private char literal[] = new char[BUFF_MAX_SIZE];
     private int literalIdx = 0;
     */
}
