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
#ifndef GI_LACP_CONFIG_HPP
#define GI_LACP_CONFIG_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace lacp {

class Config
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Config
     */
    static const opflex::modb::class_id_t CLASS_ID = 80;

    /**
     * Check whether controlBits has been set
     * @return true if controlBits has been set
     */
    bool isControlBitsSet()
    {
        return getObjectInstance().isSet(2621444ul, opflex::modb::PropertyInfo::U64);
    }

    /**
     * Get the value of controlBits if it has been set.
     * @return the value of controlBits or boost::none if not set
     */
    boost::optional<const uint8_t> getControlBits()
    {
        if (isControlBitsSet())
            return (const uint8_t)getObjectInstance().getUInt64(2621444ul);
        return boost::none;
    }

    /**
     * Get the value of controlBits if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of controlBits if set, otherwise the value of default passed in
     */
    const uint8_t getControlBits(const uint8_t defaultValue)
    {
        return getControlBits().get_value_or(defaultValue);
    }

    /**
     * Set controlBits to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lacp::Config& setControlBits(const uint8_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(2621444ul, newValue);
        return *this;
    }

    /**
     * Unset controlBits in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lacp::Config& unsetControlBits()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(2621444ul, opflex::modb::PropertyInfo::U64, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether maxLinks has been set
     * @return true if maxLinks has been set
     */
    bool isMaxLinksSet()
    {
        return getObjectInstance().isSet(2621442ul, opflex::modb::PropertyInfo::U64);
    }

    /**
     * Get the value of maxLinks if it has been set.
     * @return the value of maxLinks or boost::none if not set
     */
    boost::optional<uint16_t> getMaxLinks()
    {
        if (isMaxLinksSet())
            return (uint16_t)getObjectInstance().getUInt64(2621442ul);
        return boost::none;
    }

    /**
     * Get the value of maxLinks if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of maxLinks if set, otherwise the value of default passed in
     */
    uint16_t getMaxLinks(uint16_t defaultValue)
    {
        return getMaxLinks().get_value_or(defaultValue);
    }

    /**
     * Set maxLinks to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lacp::Config& setMaxLinks(uint16_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(2621442ul, newValue);
        return *this;
    }

    /**
     * Unset maxLinks in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lacp::Config& unsetMaxLinks()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(2621442ul, opflex::modb::PropertyInfo::U64, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether minLinks has been set
     * @return true if minLinks has been set
     */
    bool isMinLinksSet()
    {
        return getObjectInstance().isSet(2621441ul, opflex::modb::PropertyInfo::U64);
    }

    /**
     * Get the value of minLinks if it has been set.
     * @return the value of minLinks or boost::none if not set
     */
    boost::optional<uint16_t> getMinLinks()
    {
        if (isMinLinksSet())
            return (uint16_t)getObjectInstance().getUInt64(2621441ul);
        return boost::none;
    }

    /**
     * Get the value of minLinks if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of minLinks if set, otherwise the value of default passed in
     */
    uint16_t getMinLinks(uint16_t defaultValue)
    {
        return getMinLinks().get_value_or(defaultValue);
    }

    /**
     * Set minLinks to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lacp::Config& setMinLinks(uint16_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(2621441ul, newValue);
        return *this;
    }

    /**
     * Unset minLinks in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lacp::Config& unsetMinLinks()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(2621441ul, opflex::modb::PropertyInfo::U64, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether mode has been set
     * @return true if mode has been set
     */
    bool isModeSet()
    {
        return getObjectInstance().isSet(2621443ul, opflex::modb::PropertyInfo::ENUM8);
    }

    /**
     * Get the value of mode if it has been set.
     * @return the value of mode or boost::none if not set
     */
    boost::optional<const uint8_t> getMode()
    {
        if (isModeSet())
            return (const uint8_t)getObjectInstance().getUInt64(2621443ul);
        return boost::none;
    }

    /**
     * Get the value of mode if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of mode if set, otherwise the value of default passed in
     */
    const uint8_t getMode(const uint8_t defaultValue)
    {
        return getMode().get_value_or(defaultValue);
    }

    /**
     * Set mode to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lacp::Config& setMode(const uint8_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(2621443ul, newValue);
        return *this;
    }

    /**
     * Unset mode in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lacp::Config& unsetMode()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(2621443ul, opflex::modb::PropertyInfo::ENUM8, opflex::modb::PropertyInfo::SCALAR);
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
    static boost::optional<boost::shared_ptr<opmodelgbp::lacp::Config> > resolve(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::lacp::Config>(framework, CLASS_ID, uri);
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
    static boost::optional<boost::shared_ptr<opmodelgbp::lacp::Config> > resolve(
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::lacp::Config>(opflex::ofcore::OFFramework::defaultInstance(), CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of Config from the managed
     * object store by constructing its URI from the path elements
     * that lead to it.  If the object does not exist in the local
     * store, returns boost::none.  Note that even though it may not
     * exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PlatformConfig/[platformConfigName]/LacpConfig
     * 
     * @param framework the framework instance to use 
     * @param platformConfigName the value of platformConfigName,
     * a naming property for Config
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::lacp::Config> > resolve(
        opflex::ofcore::OFFramework& framework,
        const std::string& platformConfigName)
    {
        return resolve(framework,opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PlatformConfig").addElement(platformConfigName).addElement("LacpConfig").build());
    }

    /**
     * Retrieve an instance of Config from the 
     * default managed object store by constructing its URI from the
     * path elements that lead to it.  If the object does not exist in
     * the local store, returns boost::none.  Note that even though it
     * may not exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PlatformConfig/[platformConfigName]/LacpConfig
     * 
     * @param platformConfigName the value of platformConfigName,
     * a naming property for Config
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::lacp::Config> > resolve(
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
     * /PolicyUniverse/PlatformConfig/[platformConfigName]/LacpConfig
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
        MO::remove(framework, CLASS_ID, opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PlatformConfig").addElement(platformConfigName).addElement("LacpConfig").build());
    }

    /**
     * Remove the Config object with the specified path
     * elements from the managed object store using the default
     * framework instance.  If the object does not exist, then
     * this will be a no-op.  If this object has any children, they
     * will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PlatformConfig/[platformConfigName]/LacpConfig
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

} // namespace lacp
} // namespace opmodelgbp
#endif // GI_LACP_CONFIG_HPP
