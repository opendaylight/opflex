package org.opendaylight.opflex.genie.engine.format;

import org.opendaylight.opflex.genie.engine.model.Item;

/**
 * Created by midvorki on 7/24/14.
 */
public abstract class ItemFormatterTask extends FormatterTask
{
    protected ItemFormatterTask(
        FormatterCtx aInFormatterCtx,
        FileNameRule aInFileNameRule,
        Indenter aInIndenter,
        BlockFormatDirective aInHeaderFormatDirective,
        BlockFormatDirective aInCommentFormatDirective,
        boolean aInIsUserFile,
        Item aInItem)
    {
        super(aInFormatterCtx,
              aInFileNameRule,
              aInIndenter,
              aInHeaderFormatDirective,
              aInCommentFormatDirective,
              aInIsUserFile
        );

        item = aInItem;
    }

    public Item getItem()
    {
        return item;
    }

    private final Item item;
}

