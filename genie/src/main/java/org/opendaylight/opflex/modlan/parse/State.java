package org.opendaylight.opflex.modlan.parse;

/**
 * Created by midvorki on 3/10/14.
 */
public enum State
{
    //////////////////////////////////////////////////////////////////////////////////////////
    // # COMMENT
    // node<qual>:
    // {
    //     # COMMENT
    //     node:value
    //     #COMMENT
    //     node:
    //     {
    //         # COMMENT
    //         node
    //         node<qual>:value
    //     }
    // }
    //////////////////////////////////////////////////////////////////////////////////////////
    DOC(0, "doc", false, Req.NONE, Incl.SKIP, "onDocBegin", "onDocEnd", null, false),
    NODE(1, "node", true, Req.MANDATORY, Incl.DISALLOW, "onNodeBegin", "onNodeEnd", new char[]{'\n', '\r'}, false),
    QUAL(2, "qualifier", false, Req.MANDATORY, Incl.ALLOW, "onQual", null, new char[]{']'}, false),
    VALUE(3, "value", false, Req.OPTIONAL, Incl.ALLOW, "onValue", null, new char[]{'\n', '\r'}, false),
    CONTENT(4, "content", false, Req.NONE, Incl.SKIP, "onContentBegin", "onContentEnd", new char[]{'}'}, true),
    COMMENT(5, "comment", false, Req.OPTIONAL, Incl.ALLOW, "onComment", null, new char[]{'\n', '\r'}, false),
    TEXT(6, "text", false, Req.MANDATORY, Incl.ALLOW, "onText", null, new char[]{'\n', '\r'}, false)
    ;

    State(
        int aInIdx,
        String aInName,
        boolean aInIsNamed,
        Req aInTextReq,
        Incl aInBlankIncl,
        String aInBeginCb,
        String aInEndCb,
        char[] aInExitChars,
        boolean aInRecursivelyAttached
    )
    {
        idx = aInIdx;
        name = aInName;
        isNamed = aInIsNamed;
        textReq = aInTextReq;
        blankIncl = aInBlankIncl;
        beginCb = aInBeginCb;
        endCb = aInEndCb;
        exitChars = aInExitChars;
        recursivelyAttached = aInRecursivelyAttached;
    }

    public boolean isSelfContained()
    {
        switch (this)
        {
            case COMMENT:
            case TEXT:
            case VALUE:

                return true;

            default:

                return false;
        }
    }
    public String getName()
    {
        return name;
    }

    public boolean isNamed()
    {
        return isNamed;
    }

    public int getIdx()
    {
        return idx;
    }

    public Req getTextReq()
    {
        return textReq;
    }

    public Incl getBlankIncl()
    {
        return blankIncl;
    }

    public Incl getSpecialIncl()
    {
        return getBlankIncl();
    }

    public String getBeginCb()
    {
        return beginCb;
    }

    public String getEndCb()
    {
        return endCb;
    }

    public boolean isEnd(char aIn)
    {
        if (null != exitChars)
        {
            for (char lThis : exitChars)
            {
                if (aIn == lThis)
                {
                    return true;
                }
            }
        }
        return false;
    }

    public boolean isRecursivelyAttached()
    {
        return recursivelyAttached;
    }
    private final int idx;
    private final String name;
    private final boolean isNamed;
    private final Req textReq;
    private final Incl blankIncl;
    private final String beginCb;
    private final String endCb;
    private final char[] exitChars;
    private final boolean recursivelyAttached;
}
