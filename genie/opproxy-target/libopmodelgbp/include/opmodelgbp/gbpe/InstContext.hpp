/**
 * 
 * SOME COPYRIGHT
 * 
 * InstContext.hpp
 * 
 * generated InstContext.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBPE_INSTCONTEXT_HPP
#define GI_GBPE_INSTCONTEXT_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace gbpe {

class InstContext
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for InstContext
     */
    static const opflex::modb::class_id_t CLASS_ID = 23;

    /**
     * Check whether classid has been set
     * @return true if classid has been set
     */
    bool isClassidSet()
    {
        return getObjectInstance().isSet(753666ul, opflex::modb::PropertyInfo::U64);
    }

    /**
     * Get the value of classid if it has been set.
     * @return the value of classid or boost::none if not set
     */
    boost::optional<uint32_t> getClassid()
    {
        if (isClassidSet())
            return (uint32_t)getObjectInstance().getUInt64(753666ul);
        return boost::none;
    }

    /**
     * Get the value of classid if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of classid if set, otherwise the value of default passed in
     */
    uint32_t getClassid(uint32_t defaultValue)
    {
        return getClassid().get_value_or(defaultValue);
    }

    /**
     * Set classid to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::InstContext& setClassid(uint32_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(753666ul, newValue);
        return *this;
    }

    /**
     * Unset classid in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::InstContext& unsetClassid()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(753666ul, opflex::modb::PropertyInfo::U64, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether vnid has been set
     * @return true if vnid has been set
     */
    bool isVnidSet()
    {
        return getObjectInstance().isSet(753665ul, opflex::modb::PropertyInfo::U64);
    }

    /**
     * Get the value of vnid if it has been set.
     * @return the value of vnid or boost::none if not set
     */
    boost::optional<uint32_t> getVnid()
    {
        if (isVnidSet())
            return (uint32_t)getObjectInstance().getUInt64(753665ul);
        return boost::none;
    }

    /**
     * Get the value of vnid if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of vnid if set, otherwise the value of default passed in
     */
    uint32_t getVnid(uint32_t defaultValue)
    {
        return getVnid().get_value_or(defaultValue);
    }

    /**
     * Set vnid to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::InstContext& setVnid(uint32_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(753665ul, newValue);
        return *this;
    }

    /**
     * Unset vnid in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::InstContext& unsetVnid()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(753665ul, opflex::modb::PropertyInfo::U64, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Retrieve an instance of InstContext from the managed
     * object store.  If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @param framework the framework instance to use
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbpe::InstContext> > resolve(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::gbpe::InstContext>(framework, CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of InstContext from the managed
     * object store using the default framework instance.  If the 
     * object does not exist in the local store, returns boost::none. 
     * Note that even though it may not exist locally, it may still 
     * exist remotely.
     * 
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbpe::InstContext> > resolve(
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::gbpe::InstContext>(opflex::ofcore::OFFramework::defaultInstance(), CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of InstContext from the managed
     * object store by constructing its URI from the path elements
     * that lead to it.  If the object does not exist in the local
     * store, returns boost::none.  Note that even though it may not
     * exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpEpGroup/[gbpEpGroupName]/GbpeInstContext
     * 
     * @param framework the framework instance to use 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpEpGroupName the value of gbpEpGroupName,
     * a naming property for EpGroup
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbpe::InstContext> > resolveUnderPolicyUniversePolicySpaceGbpEpGroup(
        opflex::ofcore::OFFramework& framework,
        const std::string& policySpaceName,
        const std::string& gbpEpGroupName)
    {
        return resolve(framework,opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpEpGroup").addElement(gbpEpGroupName).addElement("GbpeInstContext").build());
    }

    /**
     * Retrieve an instance of InstContext from the 
     * default managed object store by constructing its URI from the
     * path elements that lead to it.  If the object does not exist in
     * the local store, returns boost::none.  Note that even though it
     * may not exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpEpGroup/[gbpEpGroupName]/GbpeInstContext
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpEpGroupName the value of gbpEpGroupName,
     * a naming property for EpGroup
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbpe::InstContext> > resolveUnderPolicyUniversePolicySpaceGbpEpGroup(
        const std::string& policySpaceName,
        const std::string& gbpEpGroupName)
    {
        return resolveUnderPolicyUniversePolicySpaceGbpEpGroup(opflex::ofcore::OFFramework::defaultInstance(),policySpaceName,gbpEpGroupName);
    }

    /**
     * Retrieve an instance of InstContext from the managed
     * object store by constructing its URI from the path elements
     * that lead to it.  If the object does not exist in the local
     * store, returns boost::none.  Note that even though it may not
     * exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpBridgeDomain/[gbpBridgeDomainName]/GbpeInstContext
     * 
     * @param framework the framework instance to use 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpBridgeDomainName the value of gbpBridgeDomainName,
     * a naming property for BridgeDomain
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbpe::InstContext> > resolveUnderPolicyUniversePolicySpaceGbpBridgeDomain(
        opflex::ofcore::OFFramework& framework,
        const std::string& policySpaceName,
        const std::string& gbpBridgeDomainName)
    {
        return resolve(framework,opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpBridgeDomain").addElement(gbpBridgeDomainName).addElement("GbpeInstContext").build());
    }

    /**
     * Retrieve an instance of InstContext from the 
     * default managed object store by constructing its URI from the
     * path elements that lead to it.  If the object does not exist in
     * the local store, returns boost::none.  Note that even though it
     * may not exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpBridgeDomain/[gbpBridgeDomainName]/GbpeInstContext
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpBridgeDomainName the value of gbpBridgeDomainName,
     * a naming property for BridgeDomain
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbpe::InstContext> > resolveUnderPolicyUniversePolicySpaceGbpBridgeDomain(
        const std::string& policySpaceName,
        const std::string& gbpBridgeDomainName)
    {
        return resolveUnderPolicyUniversePolicySpaceGbpBridgeDomain(opflex::ofcore::OFFramework::defaultInstance(),policySpaceName,gbpBridgeDomainName);
    }

    /**
     * Retrieve an instance of InstContext from the managed
     * object store by constructing its URI from the path elements
     * that lead to it.  If the object does not exist in the local
     * store, returns boost::none.  Note that even though it may not
     * exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpRoutingDomain/[gbpRoutingDomainName]/GbpeInstContext
     * 
     * @param framework the framework instance to use 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpRoutingDomainName the value of gbpRoutingDomainName,
     * a naming property for RoutingDomain
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbpe::InstContext> > resolveUnderPolicyUniversePolicySpaceGbpRoutingDomain(
        opflex::ofcore::OFFramework& framework,
        const std::string& policySpaceName,
        const std::string& gbpRoutingDomainName)
    {
        return resolve(framework,opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpRoutingDomain").addElement(gbpRoutingDomainName).addElement("GbpeInstContext").build());
    }

    /**
     * Retrieve an instance of InstContext from the 
     * default managed object store by constructing its URI from the
     * path elements that lead to it.  If the object does not exist in
     * the local store, returns boost::none.  Note that even though it
     * may not exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpRoutingDomain/[gbpRoutingDomainName]/GbpeInstContext
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpRoutingDomainName the value of gbpRoutingDomainName,
     * a naming property for RoutingDomain
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbpe::InstContext> > resolveUnderPolicyUniversePolicySpaceGbpRoutingDomain(
        const std::string& policySpaceName,
        const std::string& gbpRoutingDomainName)
    {
        return resolveUnderPolicyUniversePolicySpaceGbpRoutingDomain(opflex::ofcore::OFFramework::defaultInstance(),policySpaceName,gbpRoutingDomainName);
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
     * Remove the InstContext object with the specified URI
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
     * Remove the InstContext object with the specified URI 
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
     * Remove the InstContext object with the specified path
     * elements from the managed object store.  If the object does
     * not exist, then this will be a no-op.  If this object has any
     * children, they will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpEpGroup/[gbpEpGroupName]/GbpeInstContext
     * 
     * @param framework the framework instance to use
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpEpGroupName the value of gbpEpGroupName,
     * a naming property for EpGroup
     * @throws std::logic_error if no mutator is active
     */
    static void removeUnderPolicyUniversePolicySpaceGbpEpGroup(
        opflex::ofcore::OFFramework& framework,
        const std::string& policySpaceName,
        const std::string& gbpEpGroupName)
    {
        MO::remove(framework, CLASS_ID, opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpEpGroup").addElement(gbpEpGroupName).addElement("GbpeInstContext").build());
    }

    /**
     * Remove the InstContext object with the specified path
     * elements from the managed object store using the default
     * framework instance.  If the object does not exist, then
     * this will be a no-op.  If this object has any children, they
     * will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpEpGroup/[gbpEpGroupName]/GbpeInstContext
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpEpGroupName the value of gbpEpGroupName,
     * a naming property for EpGroup
     * @throws std::logic_error if no mutator is active
     */
    static void removeUnderPolicyUniversePolicySpaceGbpEpGroup(
        const std::string& policySpaceName,
        const std::string& gbpEpGroupName)
    {
        removeUnderPolicyUniversePolicySpaceGbpEpGroup(opflex::ofcore::OFFramework::defaultInstance(),policySpaceName,gbpEpGroupName);
    }

    /**
     * Remove the InstContext object with the specified path
     * elements from the managed object store.  If the object does
     * not exist, then this will be a no-op.  If this object has any
     * children, they will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpBridgeDomain/[gbpBridgeDomainName]/GbpeInstContext
     * 
     * @param framework the framework instance to use
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpBridgeDomainName the value of gbpBridgeDomainName,
     * a naming property for BridgeDomain
     * @throws std::logic_error if no mutator is active
     */
    static void removeUnderPolicyUniversePolicySpaceGbpBridgeDomain(
        opflex::ofcore::OFFramework& framework,
        const std::string& policySpaceName,
        const std::string& gbpBridgeDomainName)
    {
        MO::remove(framework, CLASS_ID, opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpBridgeDomain").addElement(gbpBridgeDomainName).addElement("GbpeInstContext").build());
    }

    /**
     * Remove the InstContext object with the specified path
     * elements from the managed object store using the default
     * framework instance.  If the object does not exist, then
     * this will be a no-op.  If this object has any children, they
     * will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpBridgeDomain/[gbpBridgeDomainName]/GbpeInstContext
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpBridgeDomainName the value of gbpBridgeDomainName,
     * a naming property for BridgeDomain
     * @throws std::logic_error if no mutator is active
     */
    static void removeUnderPolicyUniversePolicySpaceGbpBridgeDomain(
        const std::string& policySpaceName,
        const std::string& gbpBridgeDomainName)
    {
        removeUnderPolicyUniversePolicySpaceGbpBridgeDomain(opflex::ofcore::OFFramework::defaultInstance(),policySpaceName,gbpBridgeDomainName);
    }

    /**
     * Remove the InstContext object with the specified path
     * elements from the managed object store.  If the object does
     * not exist, then this will be a no-op.  If this object has any
     * children, they will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpRoutingDomain/[gbpRoutingDomainName]/GbpeInstContext
     * 
     * @param framework the framework instance to use
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpRoutingDomainName the value of gbpRoutingDomainName,
     * a naming property for RoutingDomain
     * @throws std::logic_error if no mutator is active
     */
    static void removeUnderPolicyUniversePolicySpaceGbpRoutingDomain(
        opflex::ofcore::OFFramework& framework,
        const std::string& policySpaceName,
        const std::string& gbpRoutingDomainName)
    {
        MO::remove(framework, CLASS_ID, opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpRoutingDomain").addElement(gbpRoutingDomainName).addElement("GbpeInstContext").build());
    }

    /**
     * Remove the InstContext object with the specified path
     * elements from the managed object store using the default
     * framework instance.  If the object does not exist, then
     * this will be a no-op.  If this object has any children, they
     * will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpRoutingDomain/[gbpRoutingDomainName]/GbpeInstContext
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpRoutingDomainName the value of gbpRoutingDomainName,
     * a naming property for RoutingDomain
     * @throws std::logic_error if no mutator is active
     */
    static void removeUnderPolicyUniversePolicySpaceGbpRoutingDomain(
        const std::string& policySpaceName,
        const std::string& gbpRoutingDomainName)
    {
        removeUnderPolicyUniversePolicySpaceGbpRoutingDomain(opflex::ofcore::OFFramework::defaultInstance(),policySpaceName,gbpRoutingDomainName);
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
     * Construct an instance of InstContext.
     * This should not typically be called from user code.
     */
    InstContext(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri,
        const boost::shared_ptr<const opflex::modb::mointernal::ObjectInstance>& oi)
        : MO(framework, CLASS_ID, uri, oi) { }
}; // class InstContext

} // namespace gbpe
} // namespace opmodelgbp
#endif // GI_GBPE_INSTCONTEXT_HPP
