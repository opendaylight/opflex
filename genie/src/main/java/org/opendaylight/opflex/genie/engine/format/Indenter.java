package org.opendaylight.opflex.genie.engine.format;

/**
 * Created by midvorki on 7/23/14.
 */

import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Indenter contains information on how to indent.
 *
 * <br>
 *        EXAMPLE: create indent lookup table of size 20 for indents of 4 spaces long
 * <br>
 *        Indent lTbl = new Indent( <br>
 *                20,  // lookup table size is 20 <br>
 *                4,   // 4 spaces per indent <br>
 *                true // starts with empty ("") <br>
 *                ) ; <br>
 * <br>
 *        for (int i = 0; i < 10; i++) <br>
 *        { <br>
 *            System.out.println(i + ": \"" + lTbl.getIndent(i) + "\""); <br>
 *        } <br>
 * <br>
 *        EXAMPLE ABOVE WILL PRODUCE THE FOLLOWING OUTPUT: <br>
 *              0: "" <br>
 *              1: "    " <br>
 *              2: "        " <br>
 *              3: "             " <br>
 *              4: "                 " <br>
 *              5: "                      " <br>
 *              6: "                          " <br>
 *              7: "                               " <br>
 *              8: "                                   " <br>
 *              9: "                                        " <br>
 */
public class Indenter
{
    static final String DEFAULT_FIRST = " ";
    static final String DEFAULT_LAST = " ";
    static final String DEFAULT_COMMON = " ";

    static final String DEFAULT_FIRST_TAB = "\t";
    static final String DEFAULT_LAST_TAB = "\t";
    static final String DEFAULT_COMMON_TAB = "\t";

    public Indenter(
            String aInName,
            int aInSize,
            int aInSingleIndentSize,
            boolean aInStartWithEmpty)
    {
        this(
            aInName,
            false,
            aInSize,
            aInSingleIndentSize,
            aInStartWithEmpty);
    }

    public Indenter(
            String aInName,
            boolean aInIsTab,
            int aInSize,
            int aInSingleIndentSize,
            boolean aInStartWithEmpty)
    {
        this(aInName,
             aInIsTab,
             aInSize,
             aInSingleIndentSize,
             aInStartWithEmpty,
             aInIsTab ? DEFAULT_FIRST_TAB : DEFAULT_FIRST,
             aInIsTab ? DEFAULT_LAST_TAB : DEFAULT_LAST,
             aInIsTab ? DEFAULT_COMMON_TAB : DEFAULT_COMMON,
             false);
    }

    /**
     * Create Indent instance with lookup table initialized to aInSize entries
     * each of (idx * aInSingleIndentSize) white spaces long.
     */
    public Indenter(
            String aInName,
            boolean aInIsTab,
            int aInSize,
            int aInSingleIndentSize,
            boolean aInStartWithEmpty,
            String aInFirst,
            String aInLast,
            String aInCommon,
            boolean aInPrependWithIndex)
    {
        name = aInName;
        isTab = aInIsTab;
        size = aInSize;
        singleIndentSize = aInSingleIndentSize;
        highestDepthInTable = aInSize - 1;
        startWithEmpty = aInStartWithEmpty;
        indent =  create(
                    aInSize,
                    aInSingleIndentSize,
                    aInStartWithEmpty,
                    aInFirst,
                    aInLast,
                    aInCommon);

        prependIndex = aInPrependWithIndex;
    }

    /**
     * gets indent of specified depth.
     * length of this indent is (singleIndentSize * aInDepth)
     */
    public String get(int aInDepth)
    {
        if (0 > aInDepth)
        {
            return Strings.EMPTY;
        }
        if (size > aInDepth)
        {
            return indent[aInDepth];
        }
        else
        {
            if (200 < aInDepth)
            {
                throw new Error("too deep");
            }
            return indent[highestDepthInTable] +
                   get(aInDepth - highestDepthInTable);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // INTERNAL IMLEMENTATION
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * creates lookup indent table
     */
    private final String[] create(
            int aInSize,
            int aInSingleIndentSize,
            boolean aInStartWithEmpty,
            String aInFirst,
            String aInLast,
            String aInCommon)
    {

        boolean lIsDefault =  ((!isTab) && (DEFAULT_FIRST.equals(aInFirst) &&
                                         DEFAULT_LAST.equals(aInLast) &&
                                         DEFAULT_COMMON.equals(aInCommon))) ||
                               (isTab && (DEFAULT_FIRST_TAB.equals(aInFirst)) &&
                                            (DEFAULT_LAST_TAB.equals(aInLast)) &&
                                            (DEFAULT_COMMON_TAB.equals(aInCommon)));

        String[] $Tbl = new String[aInSize];
        for (int i = 0; i < aInSize; i++)
        {
            if (lIsDefault)
            {
                $Tbl[i] = getDefault(i, aInSingleIndentSize, aInStartWithEmpty);
            }
            else
            {
                $Tbl[i] = createEntry(
                        i,
                        aInSingleIndentSize,
                        aInStartWithEmpty,
                        aInFirst,
                        aInLast,
                        aInCommon);
            }
        }
        return $Tbl;
    }

    private String createEntry(
            int aInDepth,
            int aInSingleIndentSize,
            boolean aInStartWithEmpty,
            String aInFirst,
            String aInLast,
            String aInCommon)
    {
        StringBuffer lSb = new StringBuffer();
        if (0 != aInDepth)
        {
            aInDepth--;
            lSb.append(aInFirst);

            for (int i = 0; i < aInDepth; i++)
            {
                lSb.append(aInCommon);
            }
        }
        lSb.append(aInLast);
        return lSb.toString();
    }

    /**
     * default indent retriever.
     */
    private String getDefault(int aInDepth, int aInSingleIndentSize, boolean aInStartWithEmpty)
    {
        if (!aInStartWithEmpty)
        {
            aInDepth++;
        }
        if (1 < aInSingleIndentSize)
        {
            aInDepth = aInDepth * aInSingleIndentSize;
        }

        if (0 > aInDepth)
        {
            aInDepth = 0;
        }
        if (TABLE_SIZE > aInDepth)
        {
            return isTab ? TAB_TABLE[aInDepth] : SPACE_TABLE[aInDepth];
        }
        else
        {
            return (isTab ? TAB_TABLE[HIGHEST] : SPACE_TABLE[HIGHEST]) +
                   getDefault(
                           aInDepth - HIGHEST,
                           1,
                           false);
        }
    }

    public String toString()
    {
        return "Indenter[" + name + "]";
    }
    /**
     * DEFAULT SINGLE SPACED INDENT TABLE
     */
    private static final String[] SPACE_TABLE =
            {
                    "", // 0
                    " ", // 1
                    "  ", // 2
                    "   ", // 3
                    "    ", // 4
                    "     ", // 5
                    "      ", // 6
                    "       ", // 7
                    "        ", // 8
                    "         ", // 9
                    "          ", // 10
                    "           ", // 11
                    "            ", // 12
                    "             ", // 13
                    "              ", // 14
                    "               ", // 15
                    "                ", // 16
                    "                 ", // 17
                    "                  ", // 18
                    "                   ", // 19
                    "                    ", // 20
            };

    /**
     * DEFAULT SINGLE TAB INDENT TABLE
     */
    private static final String[] TAB_TABLE =
            {
                    "", // 0
                    "\t", // 1
                    "\t\t", // 2
                    "\t\t\t", // 3
                    "\t\t\t\t", // 4
                    "\t\t\t\t\t", // 5
                    "\t\t\t\t\t\t", // 6
                    "\t\t\t\t\t\t\t", // 7
                    "\t\t\t\t\t\t\t\t", // 8
                    "\t\t\t\t\t\t\t\t\t", // 9
                    "\t\t\t\t\t\t\t\t\t\t", // 10
                    "\t\t\t\t\t\t\t\t\t\t\t", // 11
                    "\t\t\t\t\t\t\t\t\t\t\t\t", // 12
                    "\t\t\t\t\t\t\t\t\t\t\t\t\t", // 13
                    "\t\t\t\t\t\t\t\t\t\t\t\t\t\t", // 14
                    "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t", // 15
                    "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t", // 16
                    "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t", // 17
                    "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t", // 18
                    "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t", // 19
                    "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t", // 20
            };

    private static final int TABLE_SIZE = SPACE_TABLE.length;
    private static final int HIGHEST = TABLE_SIZE - 1;

    private final String name;
    private final String[] indent;
    private final int size;
    private final int singleIndentSize;
    private final int highestDepthInTable;
    private final boolean startWithEmpty;
    private final boolean prependIndex;
    private final boolean isTab;
}
