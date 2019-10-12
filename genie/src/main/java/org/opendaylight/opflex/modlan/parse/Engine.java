package org.opendaylight.opflex.modlan.parse;

import java.lang.reflect.Method;
import java.util.Stack;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 3/10/14.
 */
public class Engine
{
    public Engine(Ctx aInCtx, Consumer aInConsumer)
    {
        ctx = aInCtx;
        consumer = aInConsumer;
        consumer.setEngine(this);
        initCallTbl();
    }

    public Ctx getCtx()
    {
        return ctx;
    }

    protected Consumer getConsumer()
    {
        return consumer;
    }


    public final void execute()
    {
//        reportInfo("execute", "begin");
        if (null != ctx)
        {
            literals.reset();
            stack.push(new StateCtx(state, "doc-root", null));
            stack.peek().setData(invokeBeginCb(stack.peek().getText()));
            //reportInfo("execute", "push: " + stack.peek());
            while (ctx.hasMore())
            {
                ctx.getNext();

                stateMachine();
                synchronized (this) { run++; }
            }
        }
        while (!stack.empty())
        {
            endState();
        }
//        reportInfo("execute", "end");
    }

    private void stateMachine()
    {
        char lThisChar = ctx.getThis();
        // CHECK AND HANDLE IF END OF STATE
        if (checkHandleEndState(lThisChar))
        {
            // DO NOTHING
        }
        else if (' ' == lThisChar || '\t' == lThisChar)
        {
            // skip
        }
        else if (('\n' == lThisChar) || ('\r' == lThisChar))
        {
            // skip
        }
        // CHECK AND HANDLE IF THIS CHAR IS NEXT STATE
        else if (checkHandleNextState(lThisChar, lThisChar))
        {
        }
        else
        {
            endState();
            ctx.holdThisForNext();
            //reportInfo("stateMachine", "NO END, NO BEGIN: WTF: \'" + lThisChar + "' (0x" + Integer.toHexString(lThisChar)+ ")");
        }
    }

    private boolean hasNestedData()
    {
        boolean lRet = false;
        boolean lDone = false;
        while ((!lDone) && ctx.hasMore())
        {
            char lThatChar = ctx.getNext();
            switch (lThatChar)
            {
                case ' ':

                    break;
                /*
                case '\n':
                case '\r':

                    break;
                */
                default:
                {
                    lDone = true;
                    State lNextState = getNextState(lThatChar);
                    ctx.holdThisForNext();
                    if (null != lNextState)
                    {
                        //System.out.println("fastforwarded to next potential state: " + lNextState);
                        lRet = lNextState.isRecursivelyAttached();
                    }
                }
            }
        }
        return lRet;
    }
    private boolean checkHandleEndState(char aInThisChar)
    {
        if (state.isEnd(aInThisChar))
        {
            // BEGIN CHANGE
            boolean lPopNow = !hasNestedData();

            if (lPopNow) // END CHANGE
            {
                endState();
            }
            return true;
        }
        return false;
    }

    private void endState()
    {
        //reportInfo("endState", "POP: " + stack.peek());
        stack.peek().setData(invokeEndCb(stack.peek().getText()));
        literals.reset();
        stack.pop();
        if (!stack.empty())
        {
            state = stack.peek().getState();
            //reportInfo("endState", "NEW TOP: " + stack.peek());
        }

    }
    private boolean checkHandleNextState(char aInThisChar, char aInOrigChar)
    {
        boolean lRet = false;
        State lNextState = getNextState(aInThisChar);
        if (null != lNextState && state != lNextState)
        {
            if (lNextState.isSelfContained())
            {
                if (ctx.hasMore())
                {
                    ctx.getNext();
                }
                handleLiterals(lNextState);
                if (0 < literals.getSize())
                {
                    // NOTE: NO NEED TO REMEMBER DATA FOR THIS: IT'S SELF CONTAINED
                    invokeBeginCb(lNextState, literals.toString());
                    literals.reset();
                }
                lNextState = state;
            }
            else
            {
                if (Req.NONE != lNextState.getTextReq())
                {
                    // CHECK IF STATE IS NAMED
                    if (!lNextState.isNamed())
                    {

                        // IF STATE IS NOT NAMED; ADVANCE TO THE NEXT CHAR
                        // WE NEED TO ADVANCE OVER THE CHARACTER THAT IDENTIFIED
                        // TRANSITION AND IS NOT PART OF THE LITERAL TO FOLLOW.
                        // FOR NAMED, FIRST CHARACTER IS NOT SPECIAL, AND IS PART
                        // OF LITERAL NAME
                        if (ctx.hasMore())
                        {
                            ctx.getNext();
                        }
                    }
                    handleLiterals(lNextState);
                    // IF THIS IS NAMED ITEM, WE NEED TO HOLD THE LAST CHAR
                    //if (state.isNamed())
                    {
                        ctx.holdThisForNext();
                    }
                }

                //reportInfo("checkHandleNextState", "NEW STATE: " + lNextState + "(" + literals + ")");


                state = lNextState;
                String lThisString = literals.toString();
                stack.push(new StateCtx(state, lThisString, stack.peek()));
                //reportInfo("checkHandleNextState", "PUSH: " + stack.peek());
                // INVOKE BEGIN CB AND REMEMBER DATA RETURNED
                stack.peek().setData(invokeBeginCb(lThisString));
                literals.reset();
            }
            lRet = true;
        }
        return lRet;
    }

    private Data invokeBeginCb(String aInString)
    {
        return invokeBeginCb(state, aInString);
    }

    private Data invokeBeginCb(State aInState, String aInString)
    {
        //reportInfo("invokeBeginCb", "[+++] invoking for " + aInState + " STRING: " + aInString);
        if (null != callTbl[aInState.getIdx()][0])
        {
            try
            {
                return (Data) callTbl[aInState.getIdx()][0].invoke(getConsumer(), new Object[]{aInString});
            }
            catch (Throwable e)
            {
                e.printStackTrace();
                reportDeadly("parsing " + aInState.getName() + "[" + aInString + "]", e);
            }
        }
        return null;
    }

    private Data invokeEndCb(String aInString)
    {
        return invokeEndCb(state, aInString);
    }

    private Data invokeEndCb(State aInState, String aInString)
    {
        //reportInfo("invokeEndCb", "[---] invoking for " + aInState + " STRING: " + aInString);

        if (null != callTbl[aInState.getIdx()][1])
        {
            try
            {
                return (Data) callTbl[aInState.getIdx()][1].invoke(getConsumer(), new Object[]{aInString});
            }
            catch (Throwable e)
            {
                e.printStackTrace();
                reportDeadly("parsing " + aInState.getName() + "[" + aInString + "]", e);
            }
        }
        return null;
    }

    private State getNextState(char aInTrans)
    {
        Transition lThisTransition = TransitionTable.get(state, aInTrans);
        if (null != lThisTransition)
        {
            return lThisTransition.getToState();
        }
        else
        {
            return '*' == aInTrans ? null : getNextState('*');
        }
    }

    private void handleLiterals(State aInState)
    {
        //reportInfo("handleLiterals(" + aInState + ")", "begin");

        literals.reset();
        char lThisChar = ctx.getThis();

        // FAST FORWARD TO THE END OF NAME
        while (true)
        {
            if (aInState.isEnd(lThisChar))
            {
                // END OF STATE
                //reportInfo("handleLiterals(" + aInState + ")", "DONE: @end with: " + literals);
                return;
            }
            else if (null != TransitionTable.get(aInState, lThisChar))
            {
                // NEXT STATE IS BEGINNING
                //reportInfo("handleLiterals(" + aInState + ")",
                //           "DONE: @next state '" + TransitionTable.get(aInState, lThisChar) + "' on '" + lThisChar + "' with: " + literals);
                return;
            }
            else if (Character.isLetter(lThisChar) ||
                Character.isDigit(lThisChar) ||
                '-' == lThisChar ||
                '_' == lThisChar)
            {
                literals.append(lThisChar);
            }
            else if ('\n' == lThisChar ||
                    '\r' == lThisChar)
            {
                // SKIP
            }
            else if (' ' == lThisChar ||
                     '\t' == lThisChar)
            {
                /*if (aInState.isSelfContained())
                {
                    switch (aInState.getBlankIncl())
                    {
                        case DISALLOW:

                            reportDeadly(
                                    "parsing " + aInState.getName() + "[" + literals.toString() + "]",
                                    "unexpected blank space");

                            break;

                        case SKIP:

                            // NO PARSING OF THIS CHARACTER
                            break;

                        case ALLOW:
                        default:

                            literals.append(lThisChar);
                            break;
                    }
                }
                 */
                switch (aInState.getBlankIncl())
                {
                    case DISALLOW:

                        reportDeadly(
                                "parsing " + aInState.getName() + "[" + literals.toString() + "]",
                                "unexpected blank space");

                        break;

                    case SKIP:

                        // NO PARSING OF THIS CHARACTER
                        break;

                    case ALLOW:
                    default:

                        literals.append(lThisChar);
                        break;
                }
                //literals.append(lThisChar);
            }
            else if (Incl.ALLOW == aInState.getSpecialIncl())
            {
                literals.append(lThisChar);
            }
            else
            {
                reportDeadly(
                        "parsing literal",
                        "in state " + state + "/" + aInState +
                        ", special character'" + lThisChar +
                        "' are not allowed: only letters and numbers; literal so far: " +
                        literals
                        );
            }

            if (ctx.hasMore())
            {
                lThisChar = ctx.getNext();
            }
            else
            {
                //reportInfo("handleLiterals(" + aInState + ")", "DONE: OUT OF CHARS: " + literals);
                return;
            }
        }
    }

    private String formatLocationContext()
    {
        return "(" +
                run +
                ')' +
                ctx.getFileName() +
                '(' +
                ctx.getCurrLineNum() +
                ':' +
                ctx.getCurrColumnNum() +
                '/' +
                ctx.getCurrCharNum() +
                ") STATE=" +
                state;
    }

    protected void reportDeadly(String aInCause, Throwable aInT)
    {
        Severity.DEATH.report(formatLocationContext(),"PARSE", aInCause, aInT);
    }

    protected void reportDeadly(String aInCause, String aInMessage)
    {
        Severity.DEATH.report(formatLocationContext(),"PARSE", aInCause, aInMessage);
    }

    protected void reportError(String aInCause, String aInMessage)
    {
        Severity.ERROR.report(formatLocationContext(),"PARSE", aInCause, aInMessage);
    }

    protected void reportWarning(String aInCause, String aInMessage)
    {
        Severity.WARN.report(formatLocationContext(),"PARSE", aInCause, aInMessage);
    }

    protected void reportInfo(String aInCause, String aInMessage)
    {
        Severity.INFO.report(formatLocationContext(),"PARSE", aInCause, aInMessage);
    }

    private void initCallTbl()
    {
        for (State lThisState : State.values())
        {
            if (null != lThisState.getBeginCb())
            {
                try
                {
                    callTbl[lThisState.getIdx()][0] = consumer.getClass().getMethod(lThisState.getBeginCb(), String.class);
                }
                catch(Throwable e)
                {
                    e.printStackTrace();
                    reportDeadly("init beginCb", e);
                }
            }
            if (null != lThisState.getEndCb())
            {
                try
                {
                    callTbl[lThisState.getIdx()][1] = consumer.getClass().getMethod(lThisState.getEndCb(), String.class);
                }
                catch(Throwable e)
                {
                    e.printStackTrace();
                    reportDeadly("init endCb", e);
                }
            }
        }
    }

    private final Ctx ctx;
    private final Consumer consumer;
    private final Method[][] callTbl = {
                                           { null, null,},
                                           { null, null,},
                                           { null, null },
                                           { null, null,},
                                           { null, null,},
                                           { null, null },
                                           { null, null },
                                        };
    private State state = State.DOC;
    private final Stack<StateCtx> stack = new Stack<>();

    private LitBuff literals = new LitBuff();
    int run;
}
