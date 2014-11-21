/**
 * 
 * SOME COPYRIGHT
 * 
 * L2Discovered.hpp
 * 
 * generated L2Discovered.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_EPDR_L2DISCOVERED_HPP
#define GI_EPDR_L2DISCOVERED_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(epdr/LocalL2Ep)
 */
#include "opmodelgbp/epdr/LocalL2Ep.hpp"

namespace opmodelgbp {
namespace epdr {

class L2Discovered
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for L2Discovered
     */
    static const opflex::modb::class_id_t CLASS_ID = 9;

    /**
     * Retrieve an instance of L2Discovered from the managed
     * object store.  If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @param framework the framework instance to use
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::epdr::L2Discovered> > resolve(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::epdr::L2Discovered>(framework, CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of L2Discovered from the managed
     * object store using the default framework instance.  If the 
     * object does not exist in the local store, returns boost::none. 
     * Note that even though it may not exist locally, it may still 
     * exist remotely.
     * 
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::epdr::L2Discovered> > resolve(
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::epdr::L2Discovered>(opflex::ofcore::OFFramework::defaultInstance(), CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of L2Discovered from the managed
     * object store by constructing its URI from the path elements
     * that lead to it.  If the object does not exist in the local
     * store, returns boost::none.  Note that even though it may not
     * exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /EpdrL2Discovered
     * 
     * @param framework the framework instance to use 
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::epdr::L2Discovered> > resolve(
        opflex::ofcore::OFFramework& framework)
    {
        return resolve(framework,opflex::modb::URIBuilder().addElement("EpdrL2Discovered").build());
    }

    /**
     * Retrieve an instance of L2Discovered from the 
     * default managed object store by constructing its URI from the
     * path elements that lead to it.  If the object does not exist in
     * the local store, returns boost::none.  Note that even though it
     * may not exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /EpdrL2Discovered
     * 
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::epdr::L2Discovered> > resolve(
        )
    {
        return resolve(opflex::ofcore::OFFramework::defaultInstance());
    }

    /**
     * Retrieve the child object with the specified naming
     * properties. If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @param epdrLocalL2EpUuid the value of epdrLocalL2EpUuid,
     * a naming property for LocalL2Ep
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    boost::optional<boost::shared_ptr<opmodelgbp::epdr::LocalL2Ep> > resolveEpdrLocalL2Ep(
        const std::string& epdrLocalL2EpUuid)
    {
        return opmodelgbp::epdr::LocalL2Ep::resolve(getFramework(), opflex::modb::URIBuilder(getURI()).addElement("EpdrLocalL2Ep").addElement(epdrLocalL2EpUuid).build());
    }

    /**
     * Create a new child object with the specified naming properties
     * and make it a child of this object in the currently-active
     * mutator.  If the object already exists in the store, get a
     * mutable copy of that object.  If the object already exists in
     * the mutator, get a reference to the object.
     * 
     * @param epdrLocalL2EpUuid the value of epdrLocalL2EpUuid,
     * a naming property for LocalL2Ep
     * @throws std::logic_error if no mutator is active
     * @return a shared pointer to the (possibly new) object
     */
    boost::shared_ptr<opmodelgbp::epdr::LocalL2Ep> addEpdrLocalL2Ep(
        const std::string& epdrLocalL2EpUuid)
    {
        boost::shared_ptr<opmodelgbp::epdr::LocalL2Ep> result = addChild<opmodelgbp::epdr::LocalL2Ep>(
            CLASS_ID, getURI(), 2147778597ul, 37,
            opflex::modb::URIBuilder(getURI()).addElement("EpdrLocalL2Ep").addElement(epdrLocalL2EpUuid).build()
            );
        result->setUuid(epdrLocalL2EpUuid);
        return result;
    }

    /**
     * Resolve and retrieve all of the immediate children of type
     * opmodelgbp::epdr::LocalL2Ep
     * 
     * Note that this retrieves only those children that exist in the
     * local store.  It is possible that there are other children that
     * exist remotely.
     * 
     * The resulting managed objects will be added to the result
     * vector provided.
     * 
     * @param out a reference to a vector that will receive the child
     * objects.
     */
    void resolveEpdrLocalL2Ep(/* out */ std::vector<boost::shared_ptr<opmodelgbp::epdr::LocalL2Ep> >& out)
    {
        opflex::modb::mointernal::MO::resolveChildren<opmodelgbp::epdr::LocalL2Ep>(
            getFramework(), CLASS_ID, getURI(), 2147778597ul, 37, out);
    }

    /**
     * Remove this instance using the currently-active mutator.  If
     * the object does not exist, then this will be a no-op.  If this
     * object has any children, they will be garbage-collected at some
     * future time.
     * 
     * @throws std::logic_error if no mutator is active
     */
    void remove()
    {
        getTLMutator().remove(CLASS_ID, getURI());
    }

    /**
     * Remove the L2Discovered object with the specified URI
     * using the currently-active mutator.  If the object does not exist,
     * then this will be a no-op.  If this object has any children, they
     * will be garbage-collected at some future time.
     * 
     * @param framework the framework instance to use
     * @param uri the URI of the object to remove
     * @throws std::logic_error if no mutator is active
     */
    static void remove(opflex::ofcore::OFFramework& framework,
                       const opflex::modb::URI& uri)
    {
        MO::remove(framework, CLASS_ID, uri);
    }

    /**
     * Remove the L2Discovered object with the specified URI 
     * using the currently-active mutator and the default framework 
     * instance.  If the object does not exist, then this will be a
     * no-op.  If this object has any children, they will be 
     * garbage-collected at some future time.
     * 
     * @param uri the URI of the object to remove
     * @throws std::logic_error if no mutator is active
     */
    static void remove(const opflex::modb::URI& uri)
    {
        remove(opflex::ofcore::OFFramework::defaultInstance(), uri);
    }

    /**
     * Register a listener that will get called for changes related to
     * this class.  This listener will be called for any modifications
     * of this class or any transitive children of this class.
     * 
     * @param framework the framework instance 
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     */
    static void registerListener(
        opflex::ofcore::OFFramework& framework,
        opflex::modb::ObjectListener* listener)
    {
        opflex::modb::mointernal
            ::MO::registerListener(framework, listener, CLASS_ID);
    }

    /**
     * Register a listener that will get called for changes related to
     * this class with the default framework instance.  This listener
     * will be called for any modifications of this class or any
     * transitive children of this class.
     * 
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     */
    static void registerListener(
        opflex::modb::ObjectListener* listener)
    {
        registerListener(opflex::ofcore::OFFramework::defaultInstance(), listener);
    }

    /**
     * Unregister a listener from updates to this class.
     * 
     * @param framework the framework instance 
     * @param listener The listener to unregister.
     */
    static void unregisterListener(
        opflex::ofcore::OFFramework& framework,
        opflex::modb::ObjectListener* listener)
    {
        opflex::modb::mointernal
            ::MO::unregisterListener(framework, listener, CLASS_ID);
    }

    /**
     * Unregister a listener from updates to this class from the
     * default framework instance
     * 
     * @param listener The listener to unregister.
     */
    static void unregisterListener(
        opflex::modb::ObjectListener* listener)
    {
        unregisterListener(opflex::ofcore::OFFramework::defaultInstance(), listener);
    }

    /**
     * Construct an instance of L2Discovered.
     * This should not typically be called from user code.
     */
    L2Discovered(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri,
        const boost::shared_ptr<const opflex::modb::mointernal::ObjectInstance>& oi)
        : MO(framework, CLASS_ID, uri, oi) { }
}; // class L2Discovered

} // namespace epdr
} // namespace opmodelgbp
#endif // GI_EPDR_L2DISCOVERED_HPP
