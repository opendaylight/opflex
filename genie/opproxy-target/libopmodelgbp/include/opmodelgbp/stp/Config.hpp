/**
 * 
 * SOME COPYRIGHT
 * 
 * Config.hpp
 * 
 * generated Config.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_STP_CONFIG_HPP
#define GI_STP_CONFIG_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace stp {

class Config
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Config
     */
    static const opflex::modb::class_id_t CLASS_ID = 81;

    /**
     * Check whether bpduFilter has been set
     * @return true if bpduFilter has been set
     */
    bool isBpduFilterSet()
    {
        return getObjectInstance().isSet(2654210ul, opflex::modb::PropertyInfo::ENUM8);
    }

    /**
     * Get the value of bpduFilter if it has been set.
     * @return the value of bpduFilter or boost::none if not set
     */
    boost::optional<const uint8_t> getBpduFilter()
    {
        if (isBpduFilterSet())
            return (const uint8_t)getObjectInstance().getUInt64(2654210ul);
        return boost::none;
    }

    /**
     * Get the value of bpduFilter if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of bpduFilter if set, otherwise the value of default passed in
     */
    const uint8_t getBpduFilter(const uint8_t defaultValue)
    {
        return getBpduFilter().get_value_or(defaultValue);
    }

    /**
     * Set bpduFilter to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::stp::Config& setBpduFilter(const uint8_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(2654210ul, newValue);
        return *this;
    }

    /**
     * Unset bpduFilter in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::stp::Config& unsetBpduFilter()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(2654210ul, opflex::modb::PropertyInfo::ENUM8, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether bpduGuard has been set
     * @return true if bpduGuard has been set
     */
    bool isBpduGuardSet()
    {
        return getObjectInstance().isSet(2654209ul, opflex::modb::PropertyInfo::ENUM8);
    }

    /**
     * Get the value of bpduGuard if it has been set.
     * @return the value of bpduGuard or boost::none if not set
     */
    boost::optional<const uint8_t> getBpduGuard()
    {
        if (isBpduGuardSet())
            return (const uint8_t)getObjectInstance().getUInt64(2654209ul);
        return boost::none;
    }

    /**
     * Get the value of bpduGuard if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of bpduGuard if set, otherwise the value of default passed in
     */
    const uint8_t getBpduGuard(const uint8_t defaultValue)
    {
        return getBpduGuard().get_value_or(defaultValue);
    }

    /**
     * Set bpduGuard to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::stp::Config& setBpduGuard(const uint8_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(2654209ul, newValue);
        return *this;
    }

    /**
     * Unset bpduGuard in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::stp::Config& unsetBpduGuard()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(2654209ul, opflex::modb::PropertyInfo::ENUM8, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Retrieve an instance of Config from the managed
     * object store.  If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @param framework the framework instance to use
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::stp::Config> > resolve(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::stp::Config>(framework, CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of Config from the managed
     * object store using the default framework instance.  If the 
     * object does not exist in the local store, returns boost::none. 
     * Note that even though it may not exist locally, it may still 
     * exist remotely.
     * 
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::stp::Config> > resolve(
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::stp::Config>(opflex::ofcore::OFFramework::defaultInstance(), CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of Config from the managed
     * object store by constructing its URI from the path elements
     * that lead to it.  If the object does not exist in the local
     * store, returns boost::none.  Note that even though it may not
     * exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PlatformConfig/[platformConfigName]/StpConfig
     * 
     * @param framework the framework instance to use 
     * @param platformConfigName the value of platformConfigName,
     * a naming property for Config
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::stp::Config> > resolve(
        opflex::ofcore::OFFramework& framework,
        const std::string& platformConfigName)
    {
        return resolve(framework,opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PlatformConfig").addElement(platformConfigName).addElement("StpConfig").build());
    }

    /**
     * Retrieve an instance of Config from the 
     * default managed object store by constructing its URI from the
     * path elements that lead to it.  If the object does not exist in
     * the local store, returns boost::none.  Note that even though it
     * may not exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PlatformConfig/[platformConfigName]/StpConfig
     * 
     * @param platformConfigName the value of platformConfigName,
     * a naming property for Config
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::stp::Config> > resolve(
        const std::string& platformConfigName)
    {
        return resolve(opflex::ofcore::OFFramework::defaultInstance(),platformConfigName);
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
     * Remove the Config object with the specified URI
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
     * Remove the Config object with the specified URI 
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
     * Remove the Config object with the specified path
     * elements from the managed object store.  If the object does
     * not exist, then this will be a no-op.  If this object has any
     * children, they will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PlatformConfig/[platformConfigName]/StpConfig
     * 
     * @param framework the framework instance to use
     * @param platformConfigName the value of platformConfigName,
     * a naming property for Config
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        opflex::ofcore::OFFramework& framework,
        const std::string& platformConfigName)
    {
        MO::remove(framework, CLASS_ID, opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PlatformConfig").addElement(platformConfigName).addElement("StpConfig").build());
    }

    /**
     * Remove the Config object with the specified path
     * elements from the managed object store using the default
     * framework instance.  If the object does not exist, then
     * this will be a no-op.  If this object has any children, they
     * will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PlatformConfig/[platformConfigName]/StpConfig
     * 
     * @param platformConfigName the value of platformConfigName,
     * a naming property for Config
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        const std::string& platformConfigName)
    {
        remove(opflex::ofcore::OFFramework::defaultInstance(),platformConfigName);
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
     * Construct an instance of Config.
     * This should not typically be called from user code.
     */
    Config(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri,
        const boost::shared_ptr<const opflex::modb::mointernal::ObjectInstance>& oi)
        : MO(framework, CLASS_ID, uri, oi) { }
}; // class Config

} // namespace stp
} // namespace opmodelgbp
#endif // GI_STP_CONFIG_HPP
