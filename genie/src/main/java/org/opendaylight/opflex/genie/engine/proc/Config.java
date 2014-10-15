package org.opendaylight.opflex.genie.engine.proc;

import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 10/8/14.
 */
public class Config
{
    public static String CONFIG_FILE_NAME = "genie.cfg";

    public static String getProjName() { return projName; }
    public static String getLibName() { return libName; }
    public static String getHomePath() { return homePath; }
    public static String getWorkingPath() { return workingPath; }
    public static String getConfigPath() { return configPath; }
    public static String getSyntaxPath() { return syntaxPath; }
    public static String getSyntaxSuffix() { return syntaxSuffix; }
    public static String getLoaderPath() { return loaderPath; }
    public static String getLoaderSuffix() { return loaderSuffix; }
    public static String getGenDestPath() { return genDestPath; }
    public static String getLogDirParent() { return logDirParent; }

    public static void setLibName(String aIn)
    {
        projName = aIn;
        libName = aIn.startsWith("lib") ? aIn : ("lib" + aIn);
    }

    public static void setHomePath(String aIn)
    {
        homePath = Strings.isAny(aIn) ? System.getProperty("user.dir") : aIn;
    }

    public static void setSyntaxRelPath(String aIn, String aInSuffix)
    {
        syntaxPath = concatPath(homePath,aIn);
        syntaxSuffix = aInSuffix;
    };

    public static void setLoaderRelPath(String aIn, String aInSuffix)
    {
        loaderPath = concatPath(homePath,aIn);
        loaderSuffix = aInSuffix;
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
        configPath = Strings.isAny(aIn) ? concatPath(System.getProperty("user.dir"), CONFIG_FILE_NAME) : aIn;
    }

    private static String concatPath(String aInP1, String aInP2)
    {
        return aInP1 + ((aInP1.endsWith("/") || aInP2.startsWith("/")) ? "" : "/") + aInP2;
    }

    public static String print()
    {
        return "genie:config(config path: " + configPath + "; syntax path: " + syntaxPath + "; loader path: " + loaderPath + ")";
    }

    public static String projName = null;
    public static String libName = null;
    public static String homePath = null;
    public static String workingPath = initWorkingPath();
    public static String syntaxPath = null;
    public static String syntaxSuffix = null;
    public static String loaderPath = null;
    public static String loaderSuffix = null;
    public static String genDestPath = null;
    public static String configPath = null;
    public static String logDirParent = null;
}
