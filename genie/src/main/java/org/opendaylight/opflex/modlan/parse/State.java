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
    DOC(0, "doc", true, false, Req.NONE, Incl.SKIP, "onDocBegin", "onDocEnd", null, false),
    NODE(1, "node", false, true, Req.MANDATORY, Incl.DISALLOW, "onNodeBegin", "onNodeEnd", new char[]{'\n', '\r'}, false),
    QUAL(2, "qualifier", false, false, Req.MANDATORY, /**Incl.DISALLOW**/Incl.ALLOW, "onQual", null, new char[]{']'}, false),
    VALUE(3, "value", false, false, Req.OPTIONAL, Incl.ALLOW, "onValue", null, new char[]{'\n', '\r'}, false),
    CONTENT(4, "content", false, false, Req.NONE, Incl.SKIP, "onContentBegin", "onContentEnd", new char[]{'}'}, true),
    COMMENT(5, "comment", false, false, Req.OPTIONAL, Incl.ALLOW, "onComment", null, new char[]{'\n', '\r'}, false),
    TEXT(6, "text", false, false, Req.MANDATORY, Incl.ALLOW, "onText", null, new char[]{'\n', '\r'}, false)
    ;

    private State(
                int aInIdx,
                String aInName,
                boolean aInIsRoot,
                boolean aInIsNamed,
                Req aInTextReq,
                Incl aInBlankIncl,
                String aInBeginCb,
                String aInEndCb,
                char aInExitChars[],
                boolean aInRecursivelyAttached
                )
    {
        idx = aInIdx;
        name = aInName;
        isRoot = aInIsRoot;
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

    public boolean isRoot()
    {
        return isRoot;
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
    private int idx;
    private String name;
    private boolean isRoot;
    private boolean isNamed;
    private Req textReq;
    private Incl blankIncl;
    private String beginCb;
    private String endCb;
    private char exitChars[];
    private boolean recursivelyAttached;
}
