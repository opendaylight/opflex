package org.opendaylight.opflex.genie.engine.format;

import org.opendaylight.opflex.genie.engine.proc.Task;

/**
 * Created by midvorki on 7/24/14.
 */
public abstract class FormatterTask implements Task
{
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // INTERNAL APIs
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * formatter writer. use like: out.println("it's a sunny day");
     */
    protected Formatter out = null;

    /**
     * CODE GENERATION HANDLE.
     */
    public abstract void generate();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // INTERNAL APIs
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    protected FormatterTask(
        FormatterCtx aInFormatterCtx,
        FileNameRule aInFileNameRule,
        Indenter aInIndenter,
        BlockFormatDirective aInHeaderFormatDirective,
        BlockFormatDirective aInCommentFormatDirective,
        boolean aInIsUserFile)
    {
        formatterCtx = aInFormatterCtx;
        fileNameRule = aInFileNameRule;
        indenter = aInIndenter;
        headerFormatDirective = aInHeaderFormatDirective;
        commentFormatDirective = aInCommentFormatDirective;
        isUserFile = aInIsUserFile;
    }

    public Indenter getIndenter() { return indenter; }

    public final void run()
    {
        init();
        generate();
        finish();
    }

    public void firstLineCb()
    {
        // DO NOTHING
    }

    public void printHeaderComment()
    {
        out.printHeaderComment(
                -1,
                out.getHeaderComments());
    }

    protected void init()
    {
        if (null == file)
        {
            file = new FormattedFile(
                    formatterCtx,
                    fileNameRule.makeSpecific(null, null),
                    !isUserFile);
        }
        if (null == out)
        {

            out = new Formatter(
                    file,
                    indenter,
                    headerFormatDirective,
                    commentFormatDirective);

            firstLineCb();
            printHeaderComment();
        }
    }

    protected void finish()
    {
        out.flush();
        out.close();
    }

    @Override
    public String toString()
    {
        return "formatter::task(" + fileNameRule.getName() + ')';
    }


    public void setMeta(FormatterTaskMeta aInMeta)
    {
        meta = aInMeta;
    }

    public FormatterTaskMeta getMeta()
    {
        return meta;
    }

    protected FormatterTaskMeta meta = null;

    private FormattedFile file = null;

    private final FormatterCtx formatterCtx;
    private final FileNameRule fileNameRule;
    private final Indenter indenter;
    private final BlockFormatDirective headerFormatDirective;
    private final BlockFormatDirective commentFormatDirective;
    private final boolean isUserFile;
}
