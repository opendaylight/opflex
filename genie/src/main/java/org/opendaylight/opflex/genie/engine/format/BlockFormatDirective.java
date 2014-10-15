package org.opendaylight.opflex.genie.engine.format;

import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/23/14.
 */
public class BlockFormatDirective
{
    public BlockFormatDirective(
            final String aInOpenerString,
            final String aInLineStartString,
            final String aInLineEndString,
            final String aInTerminatorString,
            final int aInLineIndentOffset)
    {
        openerString = aInOpenerString;
        lineStartString = aInLineStartString;
        lineEndString = aInLineEndString;
        terminatorString = aInTerminatorString;
        lineIndentOffset = aInLineIndentOffset;
    }

    public String getOpenerString()
    {
        return openerString;
    }

    public boolean hasOpenerString()
    {
        return !Strings.isEmpty(openerString);
    }

    public String getLineStartString()
    {
        return lineStartString;
    }

    public boolean hasLineStartString()
    {
        return !Strings.isEmpty(lineStartString);
    }

    public String getLineEndString()
    {
        return lineEndString;
    }

    public boolean hasLineEndString()
    {
        return !Strings.isEmpty(lineEndString);
    }

    public String getTerminatorString()
    {
        return terminatorString;
    }

    public boolean hasTerminatorString()
    {
        return !Strings.isEmpty(terminatorString);
    }

    public int getLineIndentOffset()
    {
        return lineIndentOffset;
    }

    public boolean hasIndentOffset()
    {
        return 0 < lineIndentOffset;
    }

    private final String openerString;
    private final String lineStartString;
    private final String lineEndString;
    private final String terminatorString;
    private final int lineIndentOffset;
}
