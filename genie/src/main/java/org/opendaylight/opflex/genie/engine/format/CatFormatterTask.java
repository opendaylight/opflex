package org.opendaylight.opflex.genie.engine.format;

import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.model.Cat;

/**
 * Created by midvorki on 7/24/14.
 */
public abstract class CatFormatterTask extends FormatterTask
{
    protected CatFormatterTask(Formatter aInFormatter, Cat aInCat)
    {
        super(aInFormatter);
        cat = aInCat;
    }

    protected CatFormatterTask(FormatterCtx aInFormatterCtx,
                               FileNameRule aInFileNameRule,
                               Indenter aInIndenter,
                               BlockFormatDirective aInHeaderFormatDirective,
                               BlockFormatDirective aInCommentFormatDirective,
                               boolean aInIsUserFile,
                               WriteStats aInStats,
                               Cat aInCat)
    {
        super(aInFormatterCtx,
              aInFileNameRule,
              aInIndenter,
              aInHeaderFormatDirective,
              aInCommentFormatDirective,
              aInIsUserFile,
              aInStats);

        cat = aInCat;
    }

    public Cat getCat()
    {
        return cat;
    }

    private final Cat cat;
}
