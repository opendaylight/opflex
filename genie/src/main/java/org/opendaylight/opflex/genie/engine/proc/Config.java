package org.opendaylight.opflex.genie.engine.proc;

import java.util.Collection;
import java.util.LinkedList;

import org.opendaylight.opflex.genie.engine.format.Header;
import org.opendaylight.opflex.genie.engine.format.HeaderOption;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 10/8/14.
 */
public class Config
{
    public static String CONFIG_FILE_NAME = "genie.cfg";

    public static String getProjName() { return projName; }
    public static String getLibName() { return libName; }
    public static String getLibVersion() { return libVersion; }
    public static String getLibtoolVersion() { return libtoolVersion; }
    public static String getHomePath() { return homePath; }
    public static String getWorkingPath() { return workingPath; }
    public static String getConfigPath() { return configPath; }
    public static String getGenDestPath() { return genDestPath; }
    public static String getLogDirParent() { return logDirParent; }
    public static boolean isEnumSupport() { return isEnumSupport; } // TODO:

    public static Collection<SearchPath> getSyntaxPath() { return syntaxPath; }
    public static String[][] getSyntaxPathArray()
    {
        String lRet[][] = new String[getSyntaxPath().size()][2];
        int lIdx = 0;
        for (SearchPath lSp : getSyntaxPath())
        {
            lRet[lIdx][0] = lSp.getPath();
            lRet[lIdx][1] = lSp.getSuffix();
        }
        return lRet;
    }

    public static Collection<SearchPath> getLoaderPath() { return loaderPath; }

    public static String[][] getLoaderPathArray()
    {
        String lRet[][] = new String[getLoaderPath().size()][2];
        int lIdx = 0;
        for (SearchPath lSp : getLoaderPath())
        {
            lRet[lIdx][0] = lSp.getPath();
            lRet[lIdx++][1] = lSp.getSuffix();
        }
        return lRet;
    }

    public static Collection<SearchPath> getFormatterPath() { return formatterPath; }

    public static String[][] getFormatterPathArray()
    {
        String lRet[][] = new String[getFormatterPath().size()][2];
        int lIdx = 0;
        for (SearchPath lSp : getFormatterPath())
        {
            lRet[lIdx][0] = lSp.getPath();
            lRet[lIdx++][1] = lSp.getSuffix();
        }
        return lRet;
    }

    public static String[][] getPreLoadPaths()
    {
        final int lSize = (getLoaderPath().size());
        String lRet[][] = new String[lSize][2];
        int lIdx = 0;
        for (SearchPath lSp : getLoaderPath())
        {
            lRet[lIdx][0] = lSp.getPath();
            lRet[lIdx++][1] = lSp.getSuffix();
        }
        return lRet;
    }

    public static String[][] getPostLoadPaths()
    {
        final int lSize = (getFormatterPath().size());
        String lRet[][] = new String[lSize][2];
        int lIdx = 0;
        for (SearchPath lSp : getFormatterPath())
        {
            lRet[lIdx][0] = lSp.getPath();
            lRet[lIdx++][1] = lSp.getSuffix();
        }

        return lRet;
    }

    public static void setLibName(String aIn)
    {
        projName = aIn;
        libName = aIn.startsWith("lib") ? aIn : ("lib" + aIn);
    }
    public static void setLibVersion(String libVersion) {
		Config.libVersion = libVersion;
	}
	public static void setLibtoolVersion(String libtoolVersion) {
		Config.libtoolVersion = libtoolVersion;
	}

    public static void setHomePath(String aIn)
    {
        homePath = Strings.isAny(aIn) ? System.getProperty("user.dir") : aIn;
    }

    public static void setSyntaxRelPath(String aIn, String aInSuffix)
    {
        syntaxPath.add(new SearchPath(concatPath(homePath,aIn), aInSuffix));
    };

    public static void setLoaderRelPath(String aIn, String aInSuffix)
    {
        loaderPath.add(new SearchPath(concatPath(homePath,aIn), aInSuffix));
    }

    public static void setFormatterRelPath(String aIn, String aInSuffix)
    {
        formatterPath.add(new SearchPath(concatPath(homePath,aIn), aInSuffix));
    }

    public static void setLogDirParent(String aIn)
    {
        logDirParent = aIn;
    }

    public static void setGenDestPath(String aIn)
    {
        genDestPath = concatPath(homePath,aIn);;
    }

    private static String initWorkingPath()
    {
        return System.getProperty("user.dir");
    }

    public static void setConfigFile(String aIn)
    {
        configPath = Strings.isAny(aIn) ? concatPath(System.getProperty("user.dir")  + "/configs", CONFIG_FILE_NAME) : aIn;
        System.out.println("\n\n\nAbout to start genie with config: " + configPath + "\n\n\n");

    }

    public static void setIsEnumSupport(String aIn)
    {
        setIsEnumSupportOption(
                Strings.isEmpty(aIn) ?
                        null :
                        "enums".equalsIgnoreCase(aIn) || Strings.isYes(aIn));
    }
    public static void setIsEnumSupportOption(Boolean aInOptional)
    {
        if (null != aInOptional)
        {
            setIsEnumSupport(aInOptional.booleanValue());
        }
    }

    public static void setIsEnumSupport(boolean aIn)
    {
        isEnumSupport = aIn;
    }
    private static String concatPath(String aInP1, String aInP2)
    {
        return aInP1 + ((aInP1.endsWith("/") || aInP2.startsWith("/")) ? "" : "/") + aInP2;
    }

    public static String print()
    {
        return "genie:config(config path: " + configPath + "; syntax path: " + syntaxPath + "; loader path: " + loaderPath + ")";
    }

    public static void setHeaderFormat(String aInPath)
    {
        if (!Strings.isEmpty(aInPath))
        {
            defaultHeaderFilePath = concatPath(homePath, aInPath);
            defaultHeader = new Header(defaultHeaderFilePath);
        }

    }

    public static Header getHeaderFormat()
    {
        return defaultHeader;
    }

    private static Header getDefaultHeader()
    {
        Header lHdr = new Header();


        lHdr.add("SOME COPYRIGHT");
        lHdr.add(" ");
        lHdr.add(HeaderOption.GENIE_VAR_FULL_FILE_NAME.getName());
        lHdr.add(" ");
        lHdr.add("generated " + HeaderOption.GENIE_VAR_FULL_FILE_NAME.getName() +
                    " file genie code generation framework free of license.");
        lHdr.add(" ");
        return lHdr;
    }

    public static String projName = null;
	public static String libName = null;
    public static String libVersion = null;
    public static String libtoolVersion= null;
    public static String homePath = null;
    public static String workingPath = initWorkingPath();
    public static Collection<SearchPath> syntaxPath = new LinkedList<SearchPath>();
    public static Collection<SearchPath> loaderPath = new LinkedList<SearchPath>();
    public static Collection<SearchPath> formatterPath = new LinkedList<SearchPath>();

    public static String genDestPath = null;
    public static String configPath = null;
    public static String logDirParent = null;
    public static boolean isEnumSupport = true;
    public static String defaultHeaderFilePath = null;
    public static Header defaultHeader = getDefaultHeader();
}
