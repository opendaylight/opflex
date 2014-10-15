package org.opendaylight.opflex.genie.engine.model;

/**
 * Created by midvorki on 3/31/14.
 */
public interface Validatable
{

    /**
     * invokes pre-validation functionality
     */
    void preValidateCb();

    /**
     * invokes validation functionality
     */
    void validateCb();

    /**
     * invokes post-validation functionality
     */
    void postValidateCb();

}
