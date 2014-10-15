package org.opendaylight.opflex.genie.engine.proc;

import org.opendaylight.opflex.genie.engine.format.FormatterCtx;
import org.opendaylight.opflex.genie.engine.format.FormatterRegistry;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.parse.load.LoadStage;
import org.opendaylight.opflex.genie.engine.parse.load.LoaderRegistry;
import org.opendaylight.opflex.genie.engine.parse.model.ProcessorTree;
import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 4/4/14.
 */
public class Processor
{
    /**
     * Constructor
     *
     * Creates, initializes and executes processing functions.
     *
     * @param aInParallelism indicates how many threads are to be created for processing - more threads, the better parallelism (within reason)
     * @param aInPTree processor tree, a tree of base parser processors sufficient to establish all grammar rules
     * @param aInArgs arguments that govern the behavior of the processor, arguments come in [[name]=[value]] or [name] format. The current arguments are home=[dir-location] config=[configfilename]
     */
    public Processor(
            int aInParallelism,
            ProcessorTree aInPTree,
            String[] aInArgs)
    {
        parallelism = aInParallelism;
        INSTANCE = this;
        pTree = aInPTree;
        init(aInArgs);
        process();
        Severity.end(true);
    }

    /**
     * accessor of processor instance.
     * @return instance of the processor currently in scope.
     */
    public static Processor get()
    {
        return INSTANCE;
    }

    /**
     * dispatcher accessor
     * @return dispatcher for processor tasks.
     */
    public Dsptchr getDsp()
    {
        return dsp;
    }

    /**
     * processor tree accessor
     * @return processor tree.
     */
    public ProcessorTree getPTree()
    {
        return pTree;
    }

    /**
     * initialization routine.
     * @param aInArgs arguments that govern processor behavior
     */
    private void init(String[] aInArgs)
    {

        handleHelp(aInArgs);

        Config.setHomePath(getArg(aInArgs,"home"));
        Config.setConfigFile(getArg(aInArgs,"config"));
        new LoadTarget(
                dsp,pTree,new String[]{ Config.getConfigPath(), null}, null, false);
        Severity.init(Config.getLogDirParent());

        dsp = new Dsptchr(parallelism);


        Severity.INFO.report("","", "",Config.print());
        metadataLoadPaths = new String[][] {{Config.getSyntaxPath(), Config.getSyntaxSuffix()}};
        modelPreLoadPaths = new String[][] {{Config.getLoaderPath(), Config.getLoaderSuffix()}};
        formatterCtxs = new FormatterCtx[]{new FormatterCtx("*", Config.getGenDestPath())};
    }

    /**
     * processing method. Loads, post-processes and formats the data.
     */
    private void process()
    {
        Severity.INFO.report(this.toString(), "processing", "model processing", "BEGIN");
        try
        {
            load();
            dsp.drain();
            postProcess();
            dsp.drain();
            for (FormatterCtx lCtx : formatterCtxs)
            {
                FormatterRegistry.get().process(lCtx);
            }
            dsp.drain();
            dsp.kill();
        }
        catch (Throwable lE)
        {
            Severity.ERROR.report(this.toString(), "processing", "model processing", "EXCEPTION ENCOUNTERED: " + lE);
            lE.printStackTrace();
            System.exit(666);
        }
        finally
        {
            Severity.INFO.report(this.toString(), "processing", "model processing", "END");
        }
    }

    /**
     * model post-processor
     */
    private void postProcess()
    {
        Cat.postLoad();
        Cat.validateAll();
    }

    /**
     * model loader
     */
    private void load()
    {
        Severity.INFO.report(this.toString(), "load", "model loading", "BEGIN");

        int i, j, m;
        // FIRST PRE-LOAD METADATA
        for (m = 0; m < metadataLoadPaths.length; m++)
        {
            new LoadTarget(
                    dsp,pTree,new String[]{ metadataLoadPaths[m][0]}, metadataLoadPaths[m][1], false);
            dsp.drain();
        }
        Cat.metaModelLoadComplete();

        // FIRST PRE-LOAD WHAT NEEDS TO BE LOADED
        for (i = 0; i < modelPreLoadPaths.length; i++)
        {
            new LoadTarget(
                    dsp,pTree,new String[]{ modelPreLoadPaths[i][0]}, modelPreLoadPaths[i][1], false);
            dsp.drain();
        }

        LoaderRegistry.get().process(LoadStage.PRE);

        Cat.preLoadModelComplete();

        LoaderRegistry.get().process(LoadStage.LOAD);
        dsp.drain();
        LoaderRegistry.get().process(LoadStage.POST);
        dsp.drain();
        Cat.loadModelComplete();
        Severity.INFO.report(this.toString(),"load","model loaded", "END");
    }

    /**
     * argument finder
     * @param aInArgs list of arguments in [[name]=[value]] or [name] format.
     * @param aInName name of the argument searched
     * @return value of the argument. if argument is a flag, "true" or "false" is returned depending if the flag appears in the args.
     */
    private static String getArg(String[] aInArgs, String aInName)
    {
        if (null != aInArgs)
        {
            for (String lArg : aInArgs)
            {
                if (lArg.equalsIgnoreCase(aInName))
                {
                    return "true";
                }
                else
                {
                    String lTag = aInName + "=";

                    if (lArg.startsWith(lTag))
                    {
                        return lArg.substring(lTag.length(), lArg.length());
                    }
                }
            }
        }
        return null;
    }

    private void handleHelp(String[] aInArgs)
    {
        if ("true".equalsIgnoreCase(getArg(aInArgs, "help")) ||
            "true".equalsIgnoreCase(getArg(aInArgs, "helme")) ||
            "true".equalsIgnoreCase(getArg(aInArgs, "--help")))
        {
            System.err.println("Genie, the code generation robot can show you a lot of love.");
            System.err.println("To make it show you more love, you can help it look for useful things");
            System.err.println("by specifying the following options:");
            System.err.println("\thome=<directory-name> to help it know if you want to look for stuff in special places.");
            System.err.println("\tconfig=<config-file-path-and-name> to help it find the config file that tells it what to load and generate.");
            System.exit(666);
        }
    }
    public String toString()
    {
        return "genie:processor";
    }
    private String metadataLoadPaths[][];
    private String modelPreLoadPaths[][];
    private final ProcessorTree pTree;
    private FormatterCtx[] formatterCtxs;
    private Dsptchr dsp;
    private static Processor INSTANCE = null;
    private int parallelism;
}
