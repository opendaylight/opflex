package org.opendaylight.opflex.genie.engine.format;

import org.opendaylight.opflex.genie.engine.file.WriteStats;

/**
 * Created by midvorki on 7/25/14.
 */
public abstract class GenericFormatterTask extends FormatterTask
{
    protected GenericFormatterTask(Formatter aInFormatter)
    {
        super(aInFormatter);
    }

    protected GenericFormatterTask(FormatterCtx aInFormatterCtx,
                                   FileNameRule aInFileNameRule,
                                   Indenter aInIndenter,
                                   BlockFormatDirective aInHeaderFormatDirective,
                                   BlockFormatDirective aInCommentFormatDirective,
                                   boolean aInIsUserFile,
                                   WriteStats aInStats)
    {
        super(aInFormatterCtx,
              aInFileNameRule,
              aInIndenter,
              aInHeaderFormatDirective,
              aInCommentFormatDirective,
              aInIsUserFile,
              aInStats);
    }

    public void generate()
    {

    }
}