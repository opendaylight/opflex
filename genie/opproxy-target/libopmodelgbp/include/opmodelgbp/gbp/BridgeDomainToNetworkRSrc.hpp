/**
 * 
 * SOME COPYRIGHT
 * 
 * BridgeDomainToNetworkRSrc.hpp
 * 
 * generated BridgeDomainToNetworkRSrc.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_BRIDGEDOMAINTONETWORKRSRC_HPP
#define GI_GBP_BRIDGEDOMAINTONETWORKRSRC_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace gbp {

class BridgeDomainToNetworkRSrc
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for BridgeDomainToNetworkRSrc
     */
    static const opflex::modb::class_id_t CLASS_ID = 48;

    /**
     * Check whether role has been set
     * @return true if role has been set
     */
    bool isRoleSet()
    {
        return getObjectInstance().isSet(1572866ul, opflex::modb::PropertyInfo::ENUM8);
    }

    /**
     * Get the value of role if it has been set.
     * @return the value of role or boost::none if not set
     */
    boost::optional<const uint8_t> getRole()
    {
        if (isRoleSet())
            return (const uint8_t)getObjectInstance().getUInt64(1572866ul);
        return boost::none;
    }

    /**
     * Get the value of role if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of role if set, otherwise the value of default passed in
     */
    const uint8_t getRole(const uint8_t defaultValue)
    {
        return getRole().get_value_or(defaultValue);
    }

    /**
     * Set role to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::BridgeDomainToNetworkRSrc& setRole(const uint8_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(1572866ul, newValue);
        return *this;
    }

    /**
     * Unset role in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::BridgeDomainToNetworkRSrc& unsetRole()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(1572866ul, opflex::modb::PropertyInfo::ENUM8, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether target has been set
     * @return true if target has been set
     */
    bool isTargetSet()
    {
        return getObjectInstance().isSet(1572867ul, opflex::modb::PropertyInfo::REFERENCE);
    }

    /**
     * Get the value of targetClass if it has been set.
     * @return the value of targetClass or boost::none if not set
     */
    boost::optional<opflex::modb::class_id_t> getTargetClass()
    {
        if (isTargetSet())
            return getObjectInstance().getReference(1572867ul).first;
        return boost::none;
    }

    /**
     * Get the value of targetURI if it has been set.
     * @return the value of targetURI or boost::none if not set
     */
    boost::optional<opflex::modb::URI> getTargetURI()
    {
        if (isTargetSet())
            return getObjectInstance().getReference(1572867ul).second;
        return boost::none;
    }

    /**
     * Get the value of targetClass if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of targetClass if set, otherwise the value of default passed in
     */
    opflex::modb::class_id_t getTargetClass(opflex::modb::class_id_t defaultValue)
    {
        return getTargetClass().get_value_or(defaultValue);
    }

    /**
     * Get the value of targetURI if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of targetURI if set, otherwise the value of default passed in
     */
    opflex::modb::URI getTargetURI(opflex::modb::URI defaultValue)
    {
        return getTargetURI().get_value_or(defaultValue);
    }

    /**
     * Set the reference to point to an instance of RoutingDomain
     * with the specified URI
     * 
     * @param uri The URI of the reference to add
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::BridgeDomainToNetworkRSrc& setTargetRoutingDomain(const opflex::modb::URI& uri)
    {
        getTLMutator().modify(getClassId(), getURI())->setReference(1572867ul, 64, uri);
        return *this;
    }

    /**
     * Set the reference to point to an instance of RoutingDomain
     * in the currently-active mutator by constructing its URI from the
     * path elements that lead to it.
     * 
     * The reference URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpRoutingDomain/[gbpRoutingDomainName]
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpRoutingDomainName the value of gbpRoutingDomainName,
     * a naming property for RoutingDomain
     * 
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::BridgeDomainToNetworkRSrc& setTargetRoutingDomain(
        const std::string& policySpaceName,
        const std::string& gbpRoutingDomainName)
    {
        getTLMutator().modify(getClassId(), getURI())->setReference(1572867ul, 64, opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpRoutingDomain").addElement(gbpRoutingDomainName).build());
        return *this;
    }

    /**
     * Unset target in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::BridgeDomainToNetworkRSrc& unsetTarget()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(1572867ul, opflex::modb::PropertyInfo::REFERENCE, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether type has been set
     * @return true if type has been set
     */
    bool isTypeSet()
    {
        return getObjectInstance().isSet(1572865ul, opflex::modb::PropertyInfo::ENUM8);
    }

    /**
     * Get the value of type if it has been set.
     * @return the value of type or boost::none if not set
     */
    boost::optional<const uint8_t> getType()
    {
        if (isTypeSet())
            return (const uint8_t)getObjectInstance().getUInt64(1572865ul);
        return boost::none;
    }

    /**
     * Get the value of type if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of type if set, otherwise the value of default passed in
     */
    const uint8_t getType(const uint8_t defaultValue)
    {
        return getType().get_value_or(defaultValue);
    }

    /**
     * Set type to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::BridgeDomainToNetworkRSrc& setType(const uint8_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(1572865ul, newValue);
        return *this;
    }

    /**
     * Unset type in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::BridgeDomainToNetworkRSrc& unsetType()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(1572865ul, opflex::modb::PropertyInfo::ENUM8, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Retrieve an instance of BridgeDomainToNetworkRSrc from the managed
     * object store.  If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @param framework the framework instance to use
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbp::BridgeDomainToNetworkRSrc> > resolve(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::gbp::BridgeDomainToNetworkRSrc>(framework, CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of BridgeDomainToNetworkRSrc from the managed
     * object store using the default framework instance.  If the 
     * object does not exist in the local store, returns boost::none. 
     * Note that even though it may not exist locally, it may still 
     * exist remotely.
     * 
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbp::BridgeDomainToNetworkRSrc> > resolve(
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::gbp::BridgeDomainToNetworkRSrc>(opflex::ofcore::OFFramework::defaultInstance(), CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of BridgeDomainToNetworkRSrc from the managed
     * object store by constructing its URI from the path elements
     * that lead to it.  If the object does not exist in the local
     * store, returns boost::none.  Note that even though it may not
     * exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpBridgeDomain/[gbpBridgeDomainName]/GbpBridgeDomainToNetworkRSrc
     * 
     * @param framework the framework instance to use 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpBridgeDomainName the value of gbpBridgeDomainName,
     * a naming property for BridgeDomain
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbp::BridgeDomainToNetworkRSrc> > resolve(
        opflex::ofcore::OFFramework& framework,
        const std::string& policySpaceName,
        const std::string& gbpBridgeDomainName)
    {
        return resolve(framework,opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpBridgeDomain").addElement(gbpBridgeDomainName).addElement("GbpBridgeDomainToNetworkRSrc").build());
    }

    /**
     * Retrieve an instance of BridgeDomainToNetworkRSrc from the 
     * default managed object store by constructing its URI from the
     * path elements that lead to it.  If the object does not exist in
     * the local store, returns boost::none.  Note that even though it
     * may not exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpBridgeDomain/[gbpBridgeDomainName]/GbpBridgeDomainToNetworkRSrc
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpBridgeDomainName the value of gbpBridgeDomainName,
     * a naming property for BridgeDomain
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbp::BridgeDomainToNetworkRSrc> > resolve(
        const std::string& policySpaceName,
        const std::string& gbpBridgeDomainName)
    {
        return resolve(opflex::ofcore::OFFramework::defaultInstance(),policySpaceName,gbpBridgeDomainName);
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
     * Remove the BridgeDomainToNetworkRSrc object with the specified URI
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
     * Remove the BridgeDomainToNetworkRSrc object with the specified URI 
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
     * Remove the BridgeDomainToNetworkRSrc object with the specified path
     * elements from the managed object store.  If the object does
     * not exist, then this will be a no-op.  If this object has any
     * children, they will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpBridgeDomain/[gbpBridgeDomainName]/GbpBridgeDomainToNetworkRSrc
     * 
     * @param framework the framework instance to use
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpBridgeDomainName the value of gbpBridgeDomainName,
     * a naming property for BridgeDomain
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        opflex::ofcore::OFFramework& framework,
        const std::string& policySpaceName,
        const std::string& gbpBridgeDomainName)
    {
        MO::remove(framework, CLASS_ID, opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpBridgeDomain").addElement(gbpBridgeDomainName).addElement("GbpBridgeDomainToNetworkRSrc").build());
    }

    /**
     * Remove the BridgeDomainToNetworkRSrc object with the specified path
     * elements from the managed object store using the default
     * framework instance.  If the object does not exist, then
     * this will be a no-op.  If this object has any children, they
     * will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpBridgeDomain/[gbpBridgeDomainName]/GbpBridgeDomainToNetworkRSrc
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpBridgeDomainName the value of gbpBridgeDomainName,
     * a naming property for BridgeDomain
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        const std::string& policySpaceName,
        const std::string& gbpBridgeDomainName)
    {
        remove(opflex::ofcore::OFFramework::defaultInstance(),policySpaceName,gbpBridgeDomainName);
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
     * Construct an instance of BridgeDomainToNetworkRSrc.
     * This should not typically be called from user code.
     */
    BridgeDomainToNetworkRSrc(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri,
        const boost::shared_ptr<const opflex::modb::mointernal::ObjectInstance>& oi)
        : MO(framework, CLASS_ID, uri, oi) { }
}; // class BridgeDomainToNetworkRSrc

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_BRIDGEDOMAINTONETWORKRSRC_HPP
