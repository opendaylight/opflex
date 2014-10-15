package org.opendaylight.opflex.genie.content.model.mconst;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by dvorkinista on 7/9/14.
 *
 * Constant behavior metadata directive. Constant action identifies instruction of how a constant clobbers previously
 * defined constant with the same name along the inheritance path. In addition, it specifies whether value of the
 * corresponding constant persists or transitions to another value.
 */
public enum ConstAction
{
    /**
     * specifies that this is an alias to another constant. think of it as synonymous or an alias.
     */
    MAPPED("mapped", "map-const"), // MAPS into another constant for value

    /**
     * specifies that this constant defines a value and clobbers any previous definition (if value is specified.)
     */
    VALUE("value", "const"),   // HAS value

    /**
     * indicates that the corresponding previously defined constant with the same name is not applicable.
     */
    REMOVE("remove", "remove-const"), // removes the constant

    /**
     * indicates that the constant has value, but the value of the constant never persists,
     * instead it goes back to the previous value
     */
    AUTO_REVERTIVE("auto-revertive", "revertive-const"), // accepts the value but reverts to some original const

    /**
     * indicates that the constant has value, but the value of the constant never persists,
     * instead it goes to the value specified in the indirection rule.
     */
    AUTO_TRANSITION("auto-transition", "transitive-const"), // automatically transition to specified const

    /**
     * specifies that the constant has value, and it serves as a default value for the properties for which is
     * constant is in scope.
     */
    DEFAULT("default", "default"), // acts as default value

    /**
     * specifies that the constant is exclusive: no peer or derived constants
     */
    EXCLUSIVE("exclusive", "exclusive"), // acts as default value

    /**
     * indicates that the constant has value, but can't be set administratively
     */
    UNSETTABLE("unsettable", "unsettable-const"), // unsettable administratively
    ;

    /**
     * Specifies if corresponding constant has explicitly identify indirection. That is whether this constant uses
     * other constant's value.
     */
    public boolean hasExplicitIndirection()
    {
        return EXCLUSIVE == this || DEFAULT == this || AUTO_TRANSITION == this  || MAPPED == this;
    }

    /**
     * Specifies if corresponding constant has implied indirection (as in reverts to previous value)
     */
    public boolean hasImplicitIndirection()
    {
        return AUTO_REVERTIVE == this;
    }

    /**
     * indicates if corresponding constant has a transient value that automatically transitions to another value.
     */
    public boolean isTransient()
    {
        return AUTO_REVERTIVE == this || AUTO_TRANSITION == this;
    }

    /**
     * identifies if corresponding constant has value.
     */
    public boolean hasValue()
    {
        switch (this)
        {
            case VALUE:
            case AUTO_REVERTIVE:
            case AUTO_TRANSITION:
            case UNSETTABLE:

                return true;

            default:

                return false;
        }
    }

    public boolean hasMandatoryValue()
    {
        switch (this)
        {
            case VALUE:
            case AUTO_REVERTIVE:
            case AUTO_TRANSITION:

                return true;

            default:

                return false;
        }
    }

    public boolean hasDirectOrIndirectValue()
    {
        return this != REMOVE;
    }
    /**
     * identifies if corresponding constant acts like an alias.
     */
    public boolean isMapped()
    {
        return EXCLUSIVE == this || DEFAULT == this || MAPPED == this;
    }

    /**
     * identifies if corresponding constant acts as a default indicator (points to a constant that provides default value)
     */
    public boolean isDefault()
    {
        return DEFAULT == this;
    }

    /**
     * constructor of const action
     * @param aIn name of the constant
     * @param aInModelName model-level name
     */
    private ConstAction(String aIn, String aInModelName)
    {
        name = aIn;
        modelName = aInModelName;
    }

    /**
     * const action name accessor
     * @return name of the const action
     */
    public String getName()
    {
        return name;
    }

    /**
     * const action model name accessor
     * @return model name of the const action
     */
    public String getModelName()
    {
        return modelName;
    }

    /**
     * stringifier
     * @return name of the const action
     */
    public String toString()
    {
        return name + "/" + modelName;
    }

    /**
     * matches const action by name
     * @param aIn name of the const action to be matched
     * @return const action that matches the name
     */
    public static ConstAction get(String aIn)
    {
        for (ConstAction lCA : ConstAction.values())
        {
            if (aIn.equalsIgnoreCase(lCA.getName()) ||
                aIn.equalsIgnoreCase(lCA.getModelName()))
            {
                return lCA;
            }
        }
        Severity.DEATH.report(
                "ConstAction",
                "get const action for name",
                "no such const action",
                "no support for " + aIn + "; actions supported: " + ConstAction.values());

        return null;
    }
    private final String name;
    private final String modelName;

}
