package org.opendaylight.opflex.modlan.parse;

/**
 * Created by midvorki on 3/10/14.
 */
public enum Transition
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
    //         node[qual]:value
    //     }
    // }
    //////////////////////////////////////////////////////////////////////////////////////////

    DOC_TO_COMMENT(State.DOC, State.COMMENT, Attach.FOLLOWING, new char[]{'#'}),
    DOC_TO_NODE(State.DOC, State.NODE, Attach.CONTAINING, new char[]{'*'}),
    NODE_TO_QUAL(State.NODE, State.QUAL, Attach.CONTAINING, new char[]{'['}),
    NODE_TO_VALUE(State.NODE, State.VALUE, Attach.CONTAINING, new char[]{':'}),
//    QUAL_TO_VALUE(Status.QUAL, Status.VALUE, Attach.CONTAINING, new char[]{':'}),
    VALUE_TO_CONTENT(State.VALUE, State.CONTENT, Attach.PRECEDING, new char[]{'{'}),
    DOC_TO_CONTENT(State.DOC, State.CONTENT, Attach.PRECEDING, new char[]{'{'}),
    NODE_TO_CONTENT(State.NODE, State.CONTENT, Attach.PRECEDING, new char[]{'{'}),
    CONTENT_TO_COMMENT(State.CONTENT, State.COMMENT, Attach.FOLLOWING, new char[]{'#'}),
    CONTENT_TO_NODE(State.CONTENT, State.NODE, Attach.CONTAINING, new char[]{'*'}),
    // --> VALUE_TO_TEXT(Status.VALUE, Status.TEXT, Attach.CONTAINING, new char[]{'"'}),
    //NODE_TO_TEXT(Status.NODE, Status.TEXT, Attach.CONTAINING, new char[]{'"'}),
    ;
    private Transition(
                  State aInFromState,
                  State aInToState,
                  Attach aInAttachTo,
                  char aInEnterChars[])
    {
        fromState = aInFromState;
        toState = aInToState;
        attachTo = aInAttachTo;
        enterChars = aInEnterChars;
    }

    public State getFromState()
    {
        return fromState;
    }

    public State getToState()
    {
        return toState;
    }

    public Attach getAttachTo()
    {
        return attachTo;
    }

    public char[] getEnterChars()
    {
        return enterChars;
    }

    private State fromState;
    private State toState;
    private Attach attachTo;
    private char enterChars[];
}