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
#ifndef GI_LLDP_CONFIG_HPP
#define GI_LLDP_CONFIG_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace lldp {

class Config
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Config
     */
    static const opflex::modb::class_id_t CLASS_ID = 79;

    /**
     * Check whether rx has been set
     * @return true if rx has been set
     */
    bool isRxSet()
    {
        return getObjectInstance().isSet(2588673ul, opflex::modb::PropertyInfo::ENUM8);
    }

    /**
     * Get the value of rx if it has been set.
     * @return the value of rx or boost::none if not set
     */
    boost::optional<const uint8_t> getRx()
    {
        if (isRxSet())
            return (const uint8_t)getObjectInstance().getUInt64(2588673ul);
        return boost::none;
    }

    /**
     * Get the value of rx if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of rx if set, otherwise the value of default passed in
     */
    const uint8_t getRx(const uint8_t defaultValue)
    {
        return getRx().get_value_or(defaultValue);
    }

    /**
     * Set rx to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lldp::Config& setRx(const uint8_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(2588673ul, newValue);
        return *this;
    }

    /**
     * Unset rx in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lldp::Config& unsetRx()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(2588673ul, opflex::modb::PropertyInfo::ENUM8, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether tx has been set
     * @return true if tx has been set
     */
    bool isTxSet()
    {
        return getObjectInstance().isSet(2588674ul, opflex::modb::PropertyInfo::ENUM8);
    }

    /**
     * Get the value of tx if it has been set.
     * @return the value of tx or boost::none if not set
     */
    boost::optional<const uint8_t> getTx()
    {
        if (isTxSet())
            return (const uint8_t)getObjectInstance().getUInt64(2588674ul);
        return boost::none;
    }

    /**
     * Get the value of tx if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of tx if set, otherwise the value of default passed in
     */
    const uint8_t getTx(const uint8_t defaultValue)
    {
        return getTx().get_value_or(defaultValue);
    }

    /**
     * Set tx to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lldp::Config& setTx(const uint8_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(2588674ul, newValue);
        return *this;
    }

    /**
     * Unset tx in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lldp::Config& unsetTx()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(2588674ul, opflex::modb::PropertyInfo::ENUM8, opflex::modb::PropertyInfo::SCALAR);
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
    static boost::optional<boost::shared_ptr<opmodelgbp::lldp::Config> > resolve(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::lldp::Config>(framework, CLASS_ID, uri);
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
    static boost::optional<boost::shared_ptr<opmodelgbp::lldp::Config> > resolve(
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::lldp::Config>(opflex::ofcore::OFFramework::defaultInstance(), CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of Config from the managed
     * object store by constructing its URI from the path elements
     * that lead to it.  If the object does not exist in the local
     * store, returns boost::none.  Note that even though it may not
     * exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PlatformConfig/[platformConfigName]/LldpConfig
     * 
     * @param framework the framework instance to use 
     * @param platformConfigName the value of platformConfigName,
     * a naming property for Config
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::lldp::Config> > resolve(
        opflex::ofcore::OFFramework& framework,
        const std::string& platformConfigName)
    {
        return resolve(framework,opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PlatformConfig").addElement(platformConfigName).addElement("LldpConfig").build());
    }

    /**
     * Retrieve an instance of Config from the 
     * default managed object store by constructing its URI from the
     * path elements that lead to it.  If the object does not exist in
     * the local store, returns boost::none.  Note that even though it
     * may not exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PlatformConfig/[platformConfigName]/LldpConfig
     * 
     * @param platformConfigName the value of platformConfigName,
     * a naming property for Config
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::lldp::Config> > resolve(
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
     * /PolicyUniverse/PlatformConfig/[platformConfigName]/LldpConfig
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
        MO::remove(framework, CLASS_ID, opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PlatformConfig").addElement(platformConfigName).addElement("LldpConfig").build());
    }

    /**
     * Remove the Config object with the specified path
     * elements from the managed object store using the default
     * framework instance.  If the object does not exist, then
     * this will be a no-op.  If this object has any children, they
     * will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PlatformConfig/[platformConfigName]/LldpConfig
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

} // namespace lldp
} // namespace opmodelgbp
#endif // GI_LLDP_CONFIG_HPP
