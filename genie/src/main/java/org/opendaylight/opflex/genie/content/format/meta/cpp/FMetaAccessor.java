package org.opendaylight.opflex.genie.content.format.meta.cpp;

import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.format.*;
import org.opendaylight.opflex.genie.engine.proc.Config;

/**
 * Created by midvorki on 10/6/14.
 */
public class FMetaAccessor
        extends GenericFormatterTask
{
    public FMetaAccessor(
            FormatterCtx aInFormatterCtx,
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

    /**
     * Optional API required by the framework for transformation of file naming rule for the corresponding
     * generated file. This method can customize the location for the generated file.
     * @param aInFnr file name rule template
     * @return transformed file name rule
     */
    public static FileNameRule transformFileNameRule(FileNameRule aInFnr)
    {
        String lOldRelativePath = aInFnr.getRelativePath();
        String lNewRelativePath = lOldRelativePath + "/include/" + Config.getProjName() + "/metadata";

        FileNameRule lFnr = new FileNameRule(
                lNewRelativePath,
                null,
                aInFnr.getFilePrefix(),
                aInFnr.getFileSuffix(),
                aInFnr.getFileExtension(),
                "metadata");

        return lFnr;
    }

    public void generate()
    {
        out.println(0, "#pragma once");
        String  lNsIfDef = Config.getProjName().toUpperCase();
        out.println(0, "#ifndef GI_" + lNsIfDef + "_METADATA_H");
        out.println(0, "#define GI_" + lNsIfDef + "_METADATA_H");

        out.println(0, "#include \"opflex/modb/ModelMetadata.h\"");

        out.println(0, "namespace " + Config.getProjName());
        out.println(0, "{");
            out.printHeaderComment(1, new String[]
                    {
                        "Retrieve the model metadata object for opflex model.",
                        "This object provides the information needed by the OpFlex framework",
                        "to work with the model.",
                        "@return a ModelMetadata object containing the metadata",
                    });
            out.println(1, "const opflex::modb::ModelMetadata& getMetadata();");
        out.println();
        out.println(0, "} // namespace " + Config.getProjName());
        out.println(0, "#endif // GI_" + lNsIfDef + "_METADATA_H");
    }
}
