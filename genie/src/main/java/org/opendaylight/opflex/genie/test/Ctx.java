package org.opendaylight.opflex.genie.test;

/**
 * Created by midvorki on 3/15/14.
 */
public class Ctx implements org.opendaylight.opflex.modlan.parse.Ctx
{
    public boolean hasMore()
    {
        return (holdThis &&  currChar < text.length()) || currChar + 1 < text.length();
    }

    public char getThis()
    {
        //if (',' == text.charAt(currChar)) { new Throwable().printStackTrace();}
        return text.charAt(currChar);
    }

    public void holdThisForNext()
    {
        //System.out.println("!!!!SET HOLD FOR NEXT (" + text.charAt(currChar) + ")!!!!");
        holdThis = true;
    }

    public char getNext()
    {
        if (holdThis)
        {
            //System.out.println("!!!!RESET HOLD FOR NEXT: RETURNING CURRENT (" + text.charAt(currChar) + ")!!!!!");
            holdThis = false;
        }
        else
        {
            currChar++;
            if ('\n' == text.charAt(currChar) || '\n' == text.charAt(currChar))
            {
                currLine++;
                currColumn = 0;
            }
            else
            {
                currColumn++;
            }
        }
        return getThis();
    }

    public String getFileName()
    {
        return "/test/memory";
    }

    public int getCurrLineNum()
    {
        return currLine;
    }
    public int getCurrColumnNum()
    {
        return currColumn;
    }
    public int getCurrCharNum()
    {
        return currChar;
    }

    private String text =
            "# who are you crazy man?\n" +
            "dvorkin<mike>:\n" +
            "{\n" +
            "    # dimensions...\n" +
            "    height:average\n" +
            "    # Blah Blah\n" +
              "    girth<guesstimate>:\"fat trucker\"\n" +
            "    details:\n" +
            "    {\n" +
            "         skill<programming>:crazymad\n" +
            "    }\n" +
            "}\n";
    private int currLine = 0;
    private int currColumn = 0;
    private int currChar = -1;
    private boolean holdThis = false;
}
