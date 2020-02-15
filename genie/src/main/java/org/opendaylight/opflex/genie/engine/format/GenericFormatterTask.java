package org.opendaylight.opflex.genie.engine.format;

/**
 * Created by midvorki on 7/25/14.
 */
public abstract class GenericFormatterTask extends FormatterTask
{
    protected GenericFormatterTask(FormatterCtx aInFormatterCtx,
                                   FileNameRule aInFileNameRule,
                                   Indenter aInIndenter,
                                   BlockFormatDirective aInHeaderFormatDirective,
                                   BlockFormatDirective aInCommentFormatDirective,
                                   boolean aInIsUserFile)
    {
        super(aInFormatterCtx,
              aInFileNameRule,
              aInIndenter,
              aInHeaderFormatDirective,
              aInCommentFormatDirective,
              aInIsUserFile
        );
    }

    public void generate()
    {

    }
}