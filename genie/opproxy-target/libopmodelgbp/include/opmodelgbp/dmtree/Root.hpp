/**
 * 
 * SOME COPYRIGHT
 * 
 * Root.hpp
 * 
 * generated Root.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_DMTREE_ROOT_HPP
#define GI_DMTREE_ROOT_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(relator/Universe)
 */
#include "opmodelgbp/relator/Universe.hpp"
/*
 * contains: item:mclass(epdr/L2Discovered)
 */
#include "opmodelgbp/epdr/L2Discovered.hpp"
/*
 * contains: item:mclass(epdr/L3Discovered)
 */
#include "opmodelgbp/epdr/L3Discovered.hpp"
/*
 * contains: item:mclass(epr/L2Universe)
 */
#include "opmodelgbp/epr/L2Universe.hpp"
/*
 * contains: item:mclass(epr/L3Universe)
 */
#include "opmodelgbp/epr/L3Universe.hpp"
/*
 * contains: item:mclass(policy/Universe)
 */
#include "opmodelgbp/policy/Universe.hpp"

namespace opmodelgbp {
namespace dmtree {

class Root
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Root
     */
    static const opflex::modb::class_id_t CLASS_ID = 1;

    /**
     * Create an instance of Root, the root element in the
     * management information tree, for the given framework instance in
     * the currently-active mutator.
     * 
     * @param framework the framework instance to use
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    static boost::shared_ptr<opmodelgbp::dmtree::Root> createRootElement(opflex::ofcore::OFFramework& framework)
    {
        return opflex::modb::mointernal::MO::createRootElement<opmodelgbp::dmtree::Root>(framework, CLASS_ID);
    }

    /**
     * Create an instance of Root, the root element in the
     * management information tree, for the default framework instance in
     * the currently-active mutator.
     * 
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    static boost::shared_ptr<opmodelgbp::dmtree::Root> createRootElement()
    {
        return createRootElement(opflex::ofcore::OFFramework::defaultInstance());;
    }

    /**
     * Retrieve an instance of Root from the managed
     * object store.  If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @param framework the framework instance to use
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::dmtree::Root> > resolve(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::dmtree::Root>(framework, CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of Root from the managed
     * object store using the default framework instance.  If the 
     * object does not exist in the local store, returns boost::none. 
     * Note that even though it may not exist locally, it may still 
     * exist remotely.
     * 
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::dmtree::Root> > resolve(
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::dmtree::Root>(opflex::ofcore::OFFramework::defaultInstance(), CLASS_ID, uri);
    }

    /**
     * Retrieve the child object with the specified naming
     * properties. If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    boost::optional<boost::shared_ptr<opmodelgbp::relator::Universe> > resolveRelatorUniverse(
        )
    {
        return opmodelgbp::relator::Universe::resolve(getFramework(), opflex::modb::URIBuilder(getURI()).addElement("RelatorUniverse").build());
    }

    /**
     * Create a new child object with the specified naming properties
     * and make it a child of this object in the currently-active
     * mutator.  If the object already exists in the store, get a
     * mutable copy of that object.  If the object already exists in
     * the mutator, get a reference to the object.
     * 
     * @throws std::logic_error if no mutator is active
     * @return a shared pointer to the (possibly new) object
     */
    boost::shared_ptr<opmodelgbp::relator::Universe> addRelatorUniverse(
        )
    {
        boost::shared_ptr<opmodelgbp::relator::Universe> result = addChild<opmodelgbp::relator::Universe>(
            CLASS_ID, getURI(), 2147516423ul, 7,
            opflex::modb::URIBuilder(getURI()).addElement("RelatorUniverse").build()
            );
        return result;
    }

    /**
     * Retrieve the child object with the specified naming
     * properties. If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    boost::optional<boost::shared_ptr<opmodelgbp::epdr::L2Discovered> > resolveEpdrL2Discovered(
        )
    {
        return opmodelgbp::epdr::L2Discovered::resolve(getFramework(), opflex::modb::URIBuilder(getURI()).addElement("EpdrL2Discovered").build());
    }

    /**
     * Create a new child object with the specified naming properties
     * and make it a child of this object in the currently-active
     * mutator.  If the object already exists in the store, get a
     * mutable copy of that object.  If the object already exists in
     * the mutator, get a reference to the object.
     * 
     * @throws std::logic_error if no mutator is active
     * @return a shared pointer to the (possibly new) object
     */
    boost::shared_ptr<opmodelgbp::epdr::L2Discovered> addEpdrL2Discovered(
        )
    {
        boost::shared_ptr<opmodelgbp::epdr::L2Discovered> result = addChild<opmodelgbp::epdr::L2Discovered>(
            CLASS_ID, getURI(), 2147516425ul, 9,
            opflex::modb::URIBuilder(getURI()).addElement("EpdrL2Discovered").build()
            );
        return result;
    }

    /**
     * Retrieve the child object with the specified naming
     * properties. If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    boost::optional<boost::shared_ptr<opmodelgbp::epdr::L3Discovered> > resolveEpdrL3Discovered(
        )
    {
        return opmodelgbp::epdr::L3Discovered::resolve(getFramework(), opflex::modb::URIBuilder(getURI()).addElement("EpdrL3Discovered").build());
    }

    /**
     * Create a new child object with the specified naming properties
     * and make it a child of this object in the currently-active
     * mutator.  If the object already exists in the store, get a
     * mutable copy of that object.  If the object already exists in
     * the mutator, get a reference to the object.
     * 
     * @throws std::logic_error if no mutator is active
     * @return a shared pointer to the (possibly new) object
     */
    boost::shared_ptr<opmodelgbp::epdr::L3Discovered> addEpdrL3Discovered(
        )
    {
        boost::shared_ptr<opmodelgbp::epdr::L3Discovered> result = addChild<opmodelgbp::epdr::L3Discovered>(
            CLASS_ID, getURI(), 2147516426ul, 10,
            opflex::modb::URIBuilder(getURI()).addElement("EpdrL3Discovered").build()
            );
        return result;
    }

    /**
     * Retrieve the child object with the specified naming
     * properties. If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    boost::optional<boost::shared_ptr<opmodelgbp::epr::L2Universe> > resolveEprL2Universe(
        )
    {
        return opmodelgbp::epr::L2Universe::resolve(getFramework(), opflex::modb::URIBuilder(getURI()).addElement("EprL2Universe").build());
    }

    /**
     * Create a new child object with the specified naming properties
     * and make it a child of this object in the currently-active
     * mutator.  If the object already exists in the store, get a
     * mutable copy of that object.  If the object already exists in
     * the mutator, get a reference to the object.
     * 
     * @throws std::logic_error if no mutator is active
     * @return a shared pointer to the (possibly new) object
     */
    boost::shared_ptr<opmodelgbp::epr::L2Universe> addEprL2Universe(
        )
    {
        boost::shared_ptr<opmodelgbp::epr::L2Universe> result = addChild<opmodelgbp::epr::L2Universe>(
            CLASS_ID, getURI(), 2147516429ul, 13,
            opflex::modb::URIBuilder(getURI()).addElement("EprL2Universe").build()
            );
        return result;
    }

    /**
     * Retrieve the child object with the specified naming
     * properties. If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    boost::optional<boost::shared_ptr<opmodelgbp::epr::L3Universe> > resolveEprL3Universe(
        )
    {
        return opmodelgbp::epr::L3Universe::resolve(getFramework(), opflex::modb::URIBuilder(getURI()).addElement("EprL3Universe").build());
    }

    /**
     * Create a new child object with the specified naming properties
     * and make it a child of this object in the currently-active
     * mutator.  If the object already exists in the store, get a
     * mutable copy of that object.  If the object already exists in
     * the mutator, get a reference to the object.
     * 
     * @throws std::logic_error if no mutator is active
     * @return a shared pointer to the (possibly new) object
     */
    boost::shared_ptr<opmodelgbp::epr::L3Universe> addEprL3Universe(
        )
    {
        boost::shared_ptr<opmodelgbp::epr::L3Universe> result = addChild<opmodelgbp::epr::L3Universe>(
            CLASS_ID, getURI(), 2147516430ul, 14,
            opflex::modb::URIBuilder(getURI()).addElement("EprL3Universe").build()
            );
        return result;
    }

    /**
     * Retrieve the child object with the specified naming
     * properties. If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    boost::optional<boost::shared_ptr<opmodelgbp::policy::Universe> > resolvePolicyUniverse(
        )
    {
        return opmodelgbp::policy::Universe::resolve(getFramework(), opflex::modb::URIBuilder(getURI()).addElement("PolicyUniverse").build());
    }

    /**
     * Create a new child object with the specified naming properties
     * and make it a child of this object in the currently-active
     * mutator.  If the object already exists in the store, get a
     * mutable copy of that object.  If the object already exists in
     * the mutator, get a reference to the object.
     * 
     * @throws std::logic_error if no mutator is active
     * @return a shared pointer to the (possibly new) object
     */
    boost::shared_ptr<opmodelgbp::policy::Universe> addPolicyUniverse(
        )
    {
        boost::shared_ptr<opmodelgbp::policy::Universe> result = addChild<opmodelgbp::policy::Universe>(
            CLASS_ID, getURI(), 2147516490ul, 74,
            opflex::modb::URIBuilder(getURI()).addElement("PolicyUniverse").build()
            );
        return result;
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
     * Remove the Root object with the specified URI
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
     * Remove the Root object with the specified URI 
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
     * Construct an instance of Root.
     * This should not typically be called from user code.
     */
    Root(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri,
        const boost::shared_ptr<const opflex::modb::mointernal::ObjectInstance>& oi)
        : MO(framework, CLASS_ID, uri, oi) { }
}; // class Root

} // namespace dmtree
} // namespace opmodelgbp
#endif // GI_DMTREE_ROOT_HPP
