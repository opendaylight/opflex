package org.opendaylight.opflex.modlan.parse;

/**
 * Created by midvorki on 3/10/14.
 */
public class TransitionTable
{
    public static Transition[] get(State aInFromState)
    {
        return get()[aInFromState.getIdx()];
    }

    public static Transition get(State aInFromState, char aInChar)
    {
        Transition[] lTrans = get(aInFromState);
        if (null != lTrans)
        {
            for (Transition lThis : lTrans)
            {
                if (null != lThis)
                {
                    /**if (Character.isLetter(aInChar) && '*' == lThis.getEnterChars()[0])
                    {
                        return lThis;
                    }**/
                    for (char lChar : lThis.getEnterChars())
                    {
                        if (lChar == aInChar)
                        {
                            return lThis;
                        }
                    }
                }
            }
        }
        return null;
    }

    public static Transition[][] get()
    {
        if (null == transitionTable)
        {
            synchronized (TransitionTable.class)
            {
                if (null == transitionTable)
                {
                    transitionTable = initTransitionTable();
                }
            }
        }
        return transitionTable;
    }

    private static Transition[][] initTransitionTable()
    {
        Transition lRet [][] =
                {
                        {null, null, null, null, null, null, null},
                        {null, null, null, null, null, null, null},
                        {null, null, null, null, null, null, null},
                        {null, null, null, null, null, null, null},
                        {null, null, null, null, null, null, null},
                        {null, null, null, null, null, null, null},
                        {null, null, null, null, null, null, null},
                };

        //System.out.println("TransitionTable.initTransitionTable(" + lRet.length + "):BEGIN");

        for (Transition lThis : Transition.values())
        {
            register(lRet, lThis);
        }
        //System.out.println("TransitionTable.initTransitionTable():DONE");
        return lRet;

    }

    private static void register(Transition aInTbl[][], Transition aIn)
    {
        /**System.out.println("TransitionTable.Register()");
        System.out.println("TransitionTable.Register(" + aIn + ")");
        System.out.println("TransitionTable.Register(" + aIn + ") : indices={" +
                           aIn.getFromState() + '/' + aIn.getFromState().getIdx() + ',' +
                           aIn.getToState() + '/' + aIn.getToState().getIdx() + "}");*/
        aInTbl[aIn.getFromState().getIdx()][aIn.getToState().getIdx()] = aIn;
        /**
        System.out.println("TransitionTable.Register(" + aIn + "):REGISTERED");**/
    }

    private static Transition[][] transitionTable = null;
}
