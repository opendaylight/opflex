package org.opendaylight.opflex.genie;

import org.opendaylight.opflex.genie.content.parse.modlan.ParseRegistry;
import org.opendaylight.opflex.genie.engine.proc.Processor;

/**
 * Created by midvorki on 3/10/14.
 */
public class Genie
{

    /**
     * Genie main method.
     * arguments supported:
     * home=[home-directory] to indicate the target home directory to look for files etc.
     * config=[config-file-name] name of the config file
     * @param aInArgs a list of arguments from command line: arguments come in [[name]=[value]] or [name] format
     */
    public static void main(String[] aInArgs)
    {
        // trigger the processing
        new Processor(
                4,
                //lMetaPaths,
                //lPrePaths,
                ParseRegistry.init(),
                aInArgs
                );

    }

}
