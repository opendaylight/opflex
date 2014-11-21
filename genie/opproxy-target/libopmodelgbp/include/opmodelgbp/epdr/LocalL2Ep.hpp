/**
 * 
 * SOME COPYRIGHT
 * 
 * LocalL2Ep.hpp
 * 
 * generated LocalL2Ep.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_EPDR_LOCALL2EP_HPP
#define GI_EPDR_LOCALL2EP_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(epdr/EndPointToGroupRSrc)
 */
#include "opmodelgbp/epdr/EndPointToGroupRSrc.hpp"

namespace opmodelgbp {
namespace epdr {

class LocalL2Ep
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for LocalL2Ep
     */
    static const opflex::modb::class_id_t CLASS_ID = 37;

    /**
     * Check whether mac has been set
     * @return true if mac has been set
     */
    bool isMacSet()
    {
        return getObjectInstance().isSet(1212418ul, opflex::modb::PropertyInfo::MAC);
    }

    /**
     * Get the value of mac if it has been set.
     * @return the value of mac or boost::none if not set
     */
    boost::optional<const opflex::modb::MAC&> getMac()
    {
        if (isMacSet())
            return getObjectInstance().getMAC(1212418ul);
        return boost::none;
    }

    /**
     * Get the value of mac if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of mac if set, otherwise the value of default passed in
     */
    const opflex::modb::MAC& getMac(const opflex::modb::MAC& defaultValue)
    {
        return getMac().get_value_or(defaultValue);
    }

    /**
     * Set mac to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epdr::LocalL2Ep& setMac(const opflex::modb::MAC& newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setMAC(1212418ul, newValue);
        return *this;
    }

    /**
     * Unset mac in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epdr::LocalL2Ep& unsetMac()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(1212418ul, opflex::modb::PropertyInfo::MAC, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether uuid has been set
     * @return true if uuid has been set
     */
    bool isUuidSet()
    {
        return getObjectInstance().isSet(1212417ul, opflex::modb::PropertyInfo::STRING);
    }

    /**
     * Get the value of uuid if it has been set.
     * @return the value of uuid or boost::none if not set
     */
    boost::optional<const std::string&> getUuid()
    {
        if (isUuidSet())
            return getObjectInstance().getString(1212417ul);
        return boost::none;
    }

    /**
     * Get the value of uuid if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of uuid if set, otherwise the value of default passed in
     */
    const std::string& getUuid(const std::string& defaultValue)
    {
        return getUuid().get_value_or(defaultValue);
    }

    /**
     * Set uuid to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epdr::LocalL2Ep& setUuid(const std::string& newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setString(1212417ul, newValue);
        return *this;
    }

    /**
     * Unset uuid in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epdr::LocalL2Ep& unsetUuid()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(1212417ul, opflex::modb::PropertyInfo::STRING, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Retrieve an instance of LocalL2Ep from the managed
     * object store.  If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @param framework the framework instance to use
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::epdr::LocalL2Ep> > resolve(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::epdr::LocalL2Ep>(framework, CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of LocalL2Ep from the managed
     * object store using the default framework instance.  If the 
     * object does not exist in the local store, returns boost::none. 
     * Note that even though it may not exist locally, it may still 
     * exist remotely.
     * 
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::epdr::LocalL2Ep> > resolve(
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::epdr::LocalL2Ep>(opflex::ofcore::OFFramework::defaultInstance(), CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of LocalL2Ep from the managed
     * object store by constructing its URI from the path elements
     * that lead to it.  If the object does not exist in the local
     * store, returns boost::none.  Note that even though it may not
     * exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /EpdrL2Discovered/EpdrLocalL2Ep/[epdrLocalL2EpUuid]
     * 
     * @param framework the framework instance to use 
     * @param epdrLocalL2EpUuid the value of epdrLocalL2EpUuid,
     * a naming property for LocalL2Ep
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::epdr::LocalL2Ep> > resolve(
        opflex::ofcore::OFFramework& framework,
        const std::string& epdrLocalL2EpUuid)
    {
        return resolve(framework,opflex::modb::URIBuilder().addElement("EpdrL2Discovered").addElement("EpdrLocalL2Ep").addElement(epdrLocalL2EpUuid).build());
    }

    /**
     * Retrieve an instance of LocalL2Ep from the 
     * default managed object store by constructing its URI from the
     * path elements that lead to it.  If the object does not exist in
     * the local store, returns boost::none.  Note that even though it
     * may not exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /EpdrL2Discovered/EpdrLocalL2Ep/[epdrLocalL2EpUuid]
     * 
     * @param epdrLocalL2EpUuid the value of epdrLocalL2EpUuid,
     * a naming property for LocalL2Ep
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::epdr::LocalL2Ep> > resolve(
        const std::string& epdrLocalL2EpUuid)
    {
        return resolve(opflex::ofcore::OFFramework::defaultInstance(),epdrLocalL2EpUuid);
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
    boost::optional<boost::shared_ptr<opmodelgbp::epdr::EndPointToGroupRSrc> > resolveEpdrEndPointToGroupRSrc(
        )
    {
        return opmodelgbp::epdr::EndPointToGroupRSrc::resolve(getFramework(), opflex::modb::URIBuilder(getURI()).addElement("EpdrEndPointToGroupRSrc").build());
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
    boost::shared_ptr<opmodelgbp::epdr::EndPointToGroupRSrc> addEpdrEndPointToGroupRSrc(
        )
    {
        boost::shared_ptr<opmodelgbp::epdr::EndPointToGroupRSrc> result = addChild<opmodelgbp::epdr::EndPointToGroupRSrc>(
            CLASS_ID, getURI(), 2148696086ul, 22,
            opflex::modb::URIBuilder(getURI()).addElement("EpdrEndPointToGroupRSrc").build()
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
     * Remove the LocalL2Ep object with the specified URI
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
     * Remove the LocalL2Ep object with the specified URI 
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
     * Remove the LocalL2Ep object with the specified path
     * elements from the managed object store.  If the object does
     * not exist, then this will be a no-op.  If this object has any
     * children, they will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /EpdrL2Discovered/EpdrLocalL2Ep/[epdrLocalL2EpUuid]
     * 
     * @param framework the framework instance to use
     * @param epdrLocalL2EpUuid the value of epdrLocalL2EpUuid,
     * a naming property for LocalL2Ep
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        opflex::ofcore::OFFramework& framework,
        const std::string& epdrLocalL2EpUuid)
    {
        MO::remove(framework, CLASS_ID, opflex::modb::URIBuilder().addElement("EpdrL2Discovered").addElement("EpdrLocalL2Ep").addElement(epdrLocalL2EpUuid).build());
    }

    /**
     * Remove the LocalL2Ep object with the specified path
     * elements from the managed object store using the default
     * framework instance.  If the object does not exist, then
     * this will be a no-op.  If this object has any children, they
     * will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /EpdrL2Discovered/EpdrLocalL2Ep/[epdrLocalL2EpUuid]
     * 
     * @param epdrLocalL2EpUuid the value of epdrLocalL2EpUuid,
     * a naming property for LocalL2Ep
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        const std::string& epdrLocalL2EpUuid)
    {
        remove(opflex::ofcore::OFFramework::defaultInstance(),epdrLocalL2EpUuid);
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
     * Construct an instance of LocalL2Ep.
     * This should not typically be called from user code.
     */
    LocalL2Ep(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri,
        const boost::shared_ptr<const opflex::modb::mointernal::ObjectInstance>& oi)
        : MO(framework, CLASS_ID, uri, oi) { }
}; // class LocalL2Ep

} // namespace epdr
} // namespace opmodelgbp
#endif // GI_EPDR_LOCALL2EP_HPP
