package org.opendaylight.opflex.genie.engine.model;

/**
 * Created by midvorki on 4/4/14.
 */
public interface ImplicitFunc
{

    public void postExplicitCb();

    public void preProcessCb();

    public void preImplicitProcessCb();

    public void implicitProcessCb();

    public void postImplicitProcessCb();

    public void postProcessCb();

    public void postInitCb();

}
