package org.opendaylight.opflex.genie.engine.format;

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

import org.opendaylight.opflex.genie.engine.file.WriteStats;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.proc.Processor;
import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 7/24/14.
 */
public class FormatterTaskMeta
{
    public FormatterTaskMeta(
            String aInName,
            FormatterTaskType aInType,
            FileTypeMeta aInFile,
            FileNameRule aInFileNameRule,
            Class<FormatterTask> aInTaskClass,
            Cat aInCatOrNull,
            boolean aInIsUser)
    {
        name = aInName;
        type = aInType;
        file = aInFile;
        fileNameRule = aInFileNameRule;
        taskClass = aInTaskClass;
        catOrNull = aInCatOrNull;
        taskConstr = initConstructor(aInType);
        acceptor = initAcceptor(aInType);
        moduleExtractor = initModuleExtractor(aInType);
        fileNameRuleTransformer = initFileNameRuleTransformer(aInType);
        isUser = aInIsUser;
    }

    private Method initFileNameRuleTransformer(FormatterTaskType aInType)
    {
        Method lMethod = null;
        try
        {
            switch (aInType)
            {
                case ITEM:
                {
                    lMethod = taskClass.getMethod("transformFileNameRule", FileNameRule.class, Item.class);
                    break;
                }
                case CATEGORY:
                {
                    lMethod = taskClass.getMethod("transformFileNameRule", FileNameRule.class, Cat.class);
                    break;
                }
                default:
                {
                    lMethod = taskClass.getMethod("transformFileNameRule", FileNameRule.class);
                    break;
                }
            }
        }
        catch (Throwable lT)
        {

        }

        return lMethod;
    }

    private Method initModuleExtractor(FormatterTaskType aInType)
    {
        Method lMethod = null;
        try
        {
            switch (aInType)
            {
                case ITEM:
                {
                    lMethod = taskClass.getMethod("getTargetModule", Item.class);
                    break;
                }
                case CATEGORY:
                {
                    lMethod = taskClass.getMethod("getTargetModule", Cat.class);
                    break;
                }
                default:
                {
                    lMethod = taskClass.getMethod("getTargetModule");
                    break;
                }
            }
        }
        catch (Throwable lT)
        {

        }

        return lMethod;
    }

    private Method initAcceptor(FormatterTaskType aInType)
    {
        Method lMethod = null;
        try
        {
            switch (aInType)
            {
                case ITEM:
                {
                    lMethod = taskClass.getMethod("shouldTriggerTask", Item.class);
                    break;
                }
                default:
                {
                    lMethod = taskClass.getMethod("shouldTriggerTask");
                    break;
                }
            }
        }
        catch (Throwable lT)
        {

        }

        return lMethod;
    }

    private Constructor<FormatterTask> initConstructor(FormatterTaskType aInType)
    {
        Constructor<FormatterTask> lConst = null;
        try
        {
            switch (aInType)
            {
                case ITEM:
                {
                    lConst = taskClass.getConstructor(
                            FormatterCtx.class,
                            FileNameRule.class,
                            Indenter.class,
                            BlockFormatDirective.class,
                            BlockFormatDirective.class,
                            boolean.class,
                            WriteStats.class,
                            Item.class);
                    break;
                }
                case CATEGORY:
                {
                    lConst = taskClass.getConstructor(
                            FormatterCtx.class,
                            FileNameRule.class,
                            Indenter.class,
                            BlockFormatDirective.class,
                            BlockFormatDirective.class,
                            boolean.class,
                            WriteStats.class,
                            Cat.class);
                    break;
                }
                case GENERIC:
                default:
                {
                    lConst = taskClass.getConstructor(
                            FormatterCtx.class,
                            FileNameRule.class,
                            Indenter.class,
                            BlockFormatDirective.class,
                            BlockFormatDirective.class,
                            boolean.class,
                            WriteStats.class);
                    break;
                }
            }
        }
        catch (Throwable lE)
        {
            Severity.DEATH.report(toString(),"process","failed to constract", lE);
        }
        return lConst;
    }

    public String getName() { return name; }
    public FormatterTaskType getType() { return type; }
    public FileTypeMeta getFile() { return file; }

    public boolean isEnabled()
    {
        return isEnabled;
    }

    public void setEnabled(boolean aIn)
    {
        isEnabled = aIn;
    }

    private String getModuleContext(Object aIn)
    {
        String lModule = null;
        if (null != moduleExtractor)
        {
            try
            {
                switch (type)
                {
                    case ITEM:

                        lModule = (String) moduleExtractor.invoke(null, aIn);
                        break;

                    case CATEGORY:

                        lModule = (String) moduleExtractor.invoke(null, aIn);
                        break;

                    default:

                        lModule = (String) moduleExtractor.invoke(null);
                        break;
                }
            }
            catch (Throwable lT)
            {
                Severity.DEATH.report(toString(),"retrieving module context", "unexpected exception encountered; check the module context/getTargetModule method on your formatter task: " + taskClass, lT);
            }

        }

        if (null == lModule)
        {
            if (null != aIn && aIn instanceof Item)
            {
                Item lItem = (Item) aIn;
                for (; null != lItem.getParent(); lItem = lItem.getParent());
                lModule = lItem.getLID().getName();
            }
        }
        return lModule;
    }

    private boolean doAccept(Object aIn)
    {
        boolean lRet = true;
        try
        {
            lRet = ((null == acceptor) ||
                    ((null == aIn) ?
                        ((Boolean) acceptor.invoke(null)).booleanValue() :
                        ((Boolean) acceptor.invoke(null, aIn)).booleanValue()));
        }
        catch (Throwable lT)
        {
            Severity.DEATH.report(toString(),"check acceptance", "unexpected exception encountered; check the acceptor/shouldTriggerTask method on your formatter task: " + taskClass, lT);
        }
        return lRet;
    }

    private FileNameRule transformFileNameRule(FileNameRule aInRule, Object aInSubjectOrNull, String aInName)
    {
        FileNameRule lR = null;
        if (null != fileNameRuleTransformer)
        {
            try
            {
                if (type == FormatterTaskType.GENERIC)
                {
                    lR = (FileNameRule) fileNameRuleTransformer.invoke(null, aInRule);
                }
                else
                {
                    lR = (FileNameRule) fileNameRuleTransformer.invoke(null, aInRule, aInSubjectOrNull);
                }
            }
            catch (Throwable lT)
            {
                Severity.DEATH.report(toString(),"file name rule transformer", "unexpected exception encountered; check the transformFileNameRule method on your formatter task: " + taskClass, lT);

            }
        }
        if (null == lR)
        {
            lR = fileNameRule.makeSpecific(getModuleContext(aInSubjectOrNull), aInName);
        }
        if (null == lR)
        {
            lR = aInRule;
        }
        return lR;
    }

    public void process(FormatterCtx aInCtx)
    {
        //System.out.println(this + ".process()");
        try
        {
            switch (type)
            {
                case ITEM:
                {
                    for (Item lItem : catOrNull.getNodes().getItemsList())
                    {
                        if (doAccept(lItem))
                        {
                            FormatterTask lTask = taskConstr.newInstance(
                                                    aInCtx,
                                                    transformFileNameRule(fileNameRule, lItem, lItem.getLID().getName()),
                                                    file.getIndenter(),
                                                    file.getHeaderFormatDirective(),
                                                    file.getCommentFormatDirective(),
                                                    isUser,
                                                    aInCtx.getStats(),
                                                    //NO NEED FOR CATEGORY: catOrNull,
                                                    lItem);
                            Processor.get().getDsp().trigger(lTask);
                        }
                        else
                        {
                            //Severity.INFO.report(toString(),"process", "task not accepted for: " + lItem, "this is normal, check the acceptor/shouldTriggerTask method on your formatter task:" + taskClass);
                        }
                    }
                    break;
                }

                case CATEGORY:
                {
                    if (doAccept(null))
                    {
                        FormatterTask lTask = taskConstr.newInstance(
                                aInCtx,
                                transformFileNameRule(fileNameRule, catOrNull, name),
                                file.getIndenter(),
                                file.getHeaderFormatDirective(),
                                file.getCommentFormatDirective(),
                                isUser,
                                aInCtx.getStats(),
                                catOrNull);

                        Processor.get().getDsp().trigger(lTask);
                    }
                    else
                    {
                        //Severity.INFO.report(toString(),"process", "task not accepted for: " + catOrNull, "this is normal, check the acceptor/shouldTriggerTask method on your formatter task:" + taskClass);
                    }

                    break;
                }
                case GENERIC:
                default:
                {
                    if (doAccept(null))
                    {
                        FormatterTask lTask = taskConstr.newInstance(
                                aInCtx,
                                transformFileNameRule(fileNameRule, null, name),
                                file.getIndenter(),
                                file.getHeaderFormatDirective(),
                                file.getCommentFormatDirective(),
                                isUser,
                                aInCtx.getStats());

                        Processor.get().getDsp().trigger(lTask);
                    }
                    else
                    {
                        //Severity.INFO.report(toString(),"process", "task not accepted", "this is normal, check the acceptor/shouldTriggerTask method on your formatter task:" + taskClass);
                    }
                    break;
                }
            }
        }
        catch (Throwable lE)
        {
            Severity.DEATH.report(toString(), "process", "failed to constract", lE);
        }
    }

    public String toString()
    {
        return "formatter:task(" + name + ')';
    }

    private final String name;
    private final FormatterTaskType type;
    private final FileTypeMeta file;
    private final FileNameRule fileNameRule;
    private final Cat catOrNull;
    private final Class<FormatterTask> taskClass;
    private final Constructor<FormatterTask> taskConstr;
    private final Method acceptor;
    private final Method moduleExtractor;
    private final Method fileNameRuleTransformer;
    private final boolean isUser;
    private boolean isEnabled = true;
}
