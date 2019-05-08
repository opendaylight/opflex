package org.opendaylight.opflex.genie.engine.format;

import org.opendaylight.opflex.genie.engine.file.WriteStats;
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
     * module path formatter. formats module path (package, namespace, ..._.
     * this methid is overridable by subclasses for customization
     * @return formatted module path
     */
    protected String formatModulePath()
    {
        return null;
    }
    /**
     * name formatter. formats name
     * this methid is overridable by subclasses for customization
     * @return formatted name
     */
    protected String formatFileName()
    {
        return null;
    }
    /**
     * description formatter
     * @return description
     */
    protected String[] formatDescription()
    {
        return null;
    }

    /**
     * CODE GENERATION HANDLE.
     */
    public abstract void generate();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // INTERNAL APIs
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    protected FormatterTask(Formatter aInFormatter)
    {
        this(
          aInFormatter.getFormattedFile().getCtx(),
          aInFormatter.getFormattedFile().getFileNameRule(),
          aInFormatter.getIndenter(),
          aInFormatter.getHeaderFormatDirective(),
          aInFormatter.getCommentFormatDirective(),
          !aInFormatter.getFormattedFile().isOverrideExisting(),
          aInFormatter.getFormattedFile().getCtx().getStats()
          );
        file = aInFormatter.getFormattedFile();
        out = aInFormatter;
    }

    protected FormatterTask(
            FormatterCtx aInFormatterCtx,
            FileNameRule aInFileNameRule,
            Indenter aInIndenter,
            BlockFormatDirective aInHeaderFormatDirective,
            BlockFormatDirective aInCommentFormatDirective,
            boolean aInIsUserFile,
            WriteStats aInStats)
    {
        formatterCtx = aInFormatterCtx;
        fileNameRule = aInFileNameRule;
        indenter = aInIndenter;
        headerFormatDirective = aInHeaderFormatDirective;
        commentFormatDirective = aInCommentFormatDirective;
        isUserFile = aInIsUserFile;
        stats = aInStats;
    }

    public FormatterCtx getFormatterCtx() { return formatterCtx; }
    public FileNameRule getFileNameRule() { return fileNameRule; }
    public Indenter getIndenter() { return indenter; }
    public BlockFormatDirective getHeaderFormatDirective() { return headerFormatDirective; }
    public BlockFormatDirective getCommentFormatDirective() { return commentFormatDirective; }
    public boolean isUserFile() { return isUserFile; }
    public WriteStats getStats() { return stats; }

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
                out.getHeaderComments(formatDescription()));
    }

    protected void init()
    {
        if (null == file)
        {
            file = new FormattedFile(
                    formatterCtx,
                    fileNameRule.makeSpecific(formatModulePath(), formatFileName()),
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
    private final WriteStats stats;
}
