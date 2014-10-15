package org.opendaylight.opflex.genie.engine.format;

import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.model.Item;

/**
 * Created by midvorki on 7/24/14.
 */
public abstract class ItemFormatterTask extends FormatterTask
{
    protected ItemFormatterTask(Formatter aInFormatter, Item aInItem)
    {
        super(aInFormatter);
        item = aInItem;
    }

    protected ItemFormatterTask(
            FormatterCtx aInFormatterCtx,
            FileNameRule aInFileNameRule,
            Indenter aInIndenter,
            BlockFormatDirective aInHeaderFormatDirective,
            BlockFormatDirective aInCommentFormatDirective,
            boolean aInIsUserFile,
            WriteStats aInStats,
            Item aInItem)
    {
        super(aInFormatterCtx,
              aInFileNameRule,
              aInIndenter,
              aInHeaderFormatDirective,
              aInCommentFormatDirective,
              aInIsUserFile,
              aInStats);

        item = aInItem;
    }

    public Item getItem()
    {
        return item;
    }

    private final Item item;
}

