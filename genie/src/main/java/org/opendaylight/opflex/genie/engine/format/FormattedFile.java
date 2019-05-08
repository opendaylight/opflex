package org.opendaylight.opflex.genie.engine.format;

import java.io.File;
import java.io.PrintWriter;

import org.opendaylight.opflex.genie.engine.file.Writer;
import org.opendaylight.opflex.modlan.report.Severity;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 7/23/14.
 */
public class FormattedFile
{
    public FormattedFile(
            FormatterCtx aInFormatterCtx,
            FileNameRule aInFileNameRule,
            boolean aInOverrideExisting)
    {
        formatterCtx = aInFormatterCtx;
        fileNameRule = aInFileNameRule;
        overrideExisting = aInOverrideExisting;
        init();
    }

    public FormattedFile(
            FormatterCtx aInCtx,
            String aInRelativePath,
            String aInModulePath,
            String aInFilePrefix,
            String aInFileName,
            String aInFileSuffix,
            String aInFileExtension,
            boolean aInOverrideExisting)
    {
        this(
          aInCtx,
          new FileNameRule(aInRelativePath,aInModulePath,aInFilePrefix,aInFileSuffix,aInFileExtension, aInFileName),
          aInOverrideExisting);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // PRINTING API
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    public void print(boolean aInValue)
    {
        writer.print(aInValue);
    }

    public void print(char aInValue)
    {
        writer.print(aInValue);
    }

    public void print(char[] aInValue)
    {
        writer.print(aInValue);
    }

    public void print(double aInValue)
    {
        writer.print(aInValue);
    }

    public void print(float aInValue)
    {
        writer.print(aInValue);
    }

    public void print(int aInValue)
    {
        writer.print(aInValue);
    }

    public void print(long aInValue)
    {
        writer.print(aInValue);
    }

    public void print(Object aInValue)
    {
        writer.print(aInValue);
    }

    public void print(String aInValue)
    {
        writer.print(aInValue);
    }

    public void print(String format, Object... args)
    {
        writer.format(format, args);
    }

    public void println()
    {
        writer.println();
    }

    public void println(boolean aInValue)
    {
        writer.println(aInValue);
    }

    public void println(char aInValue)
    {
        writer.println(aInValue);
    }

    public void println(char[] aInValue)
    {
        writer.println(aInValue);
    }

    public void println(double aInValue)
    {
        writer.println(aInValue);
    }

    public void println(float aInValue)
    {
        writer.println(aInValue);
    }

    public void println(int aInValue)
    {
        writer.println(aInValue);
    }

    public void println(long aInValue)
    {
        writer.println(aInValue);
    }

    public void println(Object aInValue)
    {
        writer.println(aInValue);
    }

    public void println(String aInValue)
    {
        writer.println(aInValue);
    }

    public void println(String format, Object... args)
    {
        writer.format(format, args);
        writer.println();
    }


    public void flush()
    {
        writer.flush();
        //System.out.println(this + ".flush()");
    }

    public void close()
    {
        writer.close();
        //System.out.println(this + ".close()");
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // DATA ACCESSORS
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    public FormatterCtx getCtx()
    {
        return formatterCtx;
    }

    public FileNameRule getFileNameRule()
    {
        return fileNameRule;
    }

    public String getFullPath()
    {
        return fullPath;
    }

    public String getDirPath()
    {
        return dirPath;
    }

    public String getFullFileName()
    {
        return fullFileName;
    }

    public String getRootPath()
    {
        return formatterCtx.getRootPath();
    }

    public String getRelativePath()
    {
        return fileNameRule.getRelativePath();
    }

    public String getModulePath()
    {
        return fileNameRule.getModulePath();
    }

    public String getFilePrefix()
    {
        return fileNameRule.getFilePrefix();
    }

    public String getFileName()
    {
        return fileNameRule.getName();
    }

    public String getFileSuffix()
    {
        return fileNameRule.getFileSuffix();
    }

    public String getFileExtension()
    {
        return fileNameRule.getFileExtension();
    }

    public boolean isOverrideExisting()
    {
        return overrideExisting;
    }

    public PrintWriter getWriter()
    {
        return writer;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // INITIALIZATION
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    private void initFullPath()
    {
        StringBuilder lSb = new StringBuilder();
        if (!Strings.isEmpty(formatterCtx.getRootPath()))
        {
            lSb.append(formatterCtx.getRootPath());
            if (!formatterCtx.getRootPath().endsWith("/"))
            {
                lSb.append('/');
            }
        }
        if (!Strings.isEmpty(getRelativePath()))
        {
            lSb.append(getRelativePath());
            if (!getRelativePath().endsWith("/"))
            {
                lSb.append('/');
            }
        }
        if (!Strings.isEmpty(getModulePath()))
        {
            lSb.append(getModulePath());
            if (!getModulePath().endsWith("/"))
            {
                lSb.append('/');
            }
        }
        dirPath = lSb.toString();

        StringBuilder lFnSb = new StringBuilder();

        if (!Strings.isEmpty(getFilePrefix()))
        {
            lFnSb.append(getFilePrefix());

        }
        if (!Strings.isEmpty(getFileName()))
        {
            lFnSb.append(getFileName());
        }
        if (!Strings.isEmpty(getFileSuffix()))
        {
            lFnSb.append(getFileSuffix());
        }
        if (!Strings.isEmpty(getFileExtension()))
        {
            if (!getFileExtension().startsWith("."))
            {
                lSb.append('.');
            }
            lFnSb.append(getFileExtension());
        }
        lSb.append(lFnSb);
        fullPath = lSb.toString();
        fullFileName = lFnSb.toString();
        //Severity.WARN.report(fileName,"","","|||||||||||||||" + this + "--> " + fullPath);
    }

    public File getDir()
    {
        return dir;
    }

    public File getFile()
    {
        return file;
    }

    private void init()
    {
        initFullPath();
        initDir();
        initFile();
        initWriter();
    }

    private synchronized void initDir()
    {
        try
        {
            dir = new File(dirPath).getCanonicalFile();
        }
        catch (Throwable lE)
        {
            Severity.DEATH.report(toString(),"initialize directory","","", lE);
        }
        for  (int i = 0; i < 10 && !dir.exists(); i++)
        {
            if (!dir.mkdirs())
            {
                try
                {
                    Thread.sleep(100);
                }
                catch (Throwable lE)
                {

                }
            }
        }
        if (!dir.exists())
        {
            Severity.DEATH.report(toString(),"initialize directory","", "unable to init directory: " + dirPath);
        }
    }


    private synchronized void initFile()
    {
        file = new File(fullPath); //new File(dir, aInFileName);
        if ((!overrideExisting) && file.exists())
        {
            file = new File(fullPath + ".new");// new File(dir, aInFileName + ".new");
        }
    }

    private synchronized void initWriter()
    {
        try
        {
            writer = new PrintWriter(new Writer(file, formatterCtx.getStats()));
        }
        catch (Throwable lE)
        {
            Severity.DEATH.report(toString(),"initialize writer","","", lE);

        }
    }

    public String toString()
    {
        return "formatted-file(" + fullPath + ")";
    }

    private String dirPath = null;
    private String fullPath = null;
    private String fullFileName = null;

    private final FormatterCtx formatterCtx;
    private final FileNameRule fileNameRule;
    private final boolean overrideExisting;
    private File dir = null;
    private File file = null;
    private PrintWriter writer = null;
}
