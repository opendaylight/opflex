/**
 * 
 * SOME COPYRIGHT
 * 
 * EpGroupFromProvContractRTgt.hpp
 * 
 * generated EpGroupFromProvContractRTgt.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_EPGROUPFROMPROVCONTRACTRTGT_HPP
#define GI_GBP_EPGROUPFROMPROVCONTRACTRTGT_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace gbp {

class EpGroupFromProvContractRTgt
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for EpGroupFromProvContractRTgt
     */
    static const opflex::modb::class_id_t CLASS_ID = 40;

    /**
     * Check whether role has been set
     * @return true if role has been set
     */
    bool isRoleSet()
    {
        return getObjectInstance().isSet(1310722ul, opflex::modb::PropertyInfo::ENUM8);
    }

    /**
     * Get the value of role if it has been set.
     * @return the value of role or boost::none if not set
     */
    boost::optional<const uint8_t> getRole()
    {
        if (isRoleSet())
            return (const uint8_t)getObjectInstance().getUInt64(1310722ul);
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
    opmodelgbp::gbp::EpGroupFromProvContractRTgt& setRole(const uint8_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(1310722ul, newValue);
        return *this;
    }

    /**
     * Unset role in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::EpGroupFromProvContractRTgt& unsetRole()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(1310722ul, opflex::modb::PropertyInfo::ENUM8, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether source has been set
     * @return true if source has been set
     */
    bool isSourceSet()
    {
        return getObjectInstance().isSet(1310723ul, opflex::modb::PropertyInfo::STRING);
    }

    /**
     * Get the value of source if it has been set.
     * @return the value of source or boost::none if not set
     */
    boost::optional<const std::string&> getSource()
    {
        if (isSourceSet())
            return getObjectInstance().getString(1310723ul);
        return boost::none;
    }

    /**
     * Get the value of source if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of source if set, otherwise the value of default passed in
     */
    const std::string& getSource(const std::string& defaultValue)
    {
        return getSource().get_value_or(defaultValue);
    }

    /**
     * Set source to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::EpGroupFromProvContractRTgt& setSource(const std::string& newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setString(1310723ul, newValue);
        return *this;
    }

    /**
     * Unset source in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::EpGroupFromProvContractRTgt& unsetSource()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(1310723ul, opflex::modb::PropertyInfo::STRING, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether type has been set
     * @return true if type has been set
     */
    bool isTypeSet()
    {
        return getObjectInstance().isSet(1310721ul, opflex::modb::PropertyInfo::ENUM8);
    }

    /**
     * Get the value of type if it has been set.
     * @return the value of type or boost::none if not set
     */
    boost::optional<const uint8_t> getType()
    {
        if (isTypeSet())
            return (const uint8_t)getObjectInstance().getUInt64(1310721ul);
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
    opmodelgbp::gbp::EpGroupFromProvContractRTgt& setType(const uint8_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(1310721ul, newValue);
        return *this;
    }

    /**
     * Unset type in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::EpGroupFromProvContractRTgt& unsetType()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(1310721ul, opflex::modb::PropertyInfo::ENUM8, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Retrieve an instance of EpGroupFromProvContractRTgt from the managed
     * object store.  If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @param framework the framework instance to use
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbp::EpGroupFromProvContractRTgt> > resolve(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::gbp::EpGroupFromProvContractRTgt>(framework, CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of EpGroupFromProvContractRTgt from the managed
     * object store using the default framework instance.  If the 
     * object does not exist in the local store, returns boost::none. 
     * Note that even though it may not exist locally, it may still 
     * exist remotely.
     * 
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbp::EpGroupFromProvContractRTgt> > resolve(
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::gbp::EpGroupFromProvContractRTgt>(opflex::ofcore::OFFramework::defaultInstance(), CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of EpGroupFromProvContractRTgt from the managed
     * object store by constructing its URI from the path elements
     * that lead to it.  If the object does not exist in the local
     * store, returns boost::none.  Note that even though it may not
     * exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpContract/[gbpContractName]/GbpEpGroupFromProvContractRTgt/[gbpEpGroupFromProvContractRTgtSource]
     * 
     * @param framework the framework instance to use 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpContractName the value of gbpContractName,
     * a naming property for Contract
     * @param gbpEpGroupFromProvContractRTgtSource the value of gbpEpGroupFromProvContractRTgtSource,
     * a naming property for EpGroupFromProvContractRTgt
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbp::EpGroupFromProvContractRTgt> > resolve(
        opflex::ofcore::OFFramework& framework,
        const std::string& policySpaceName,
        const std::string& gbpContractName,
        const std::string& gbpEpGroupFromProvContractRTgtSource)
    {
        return resolve(framework,opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpContract").addElement(gbpContractName).addElement("GbpEpGroupFromProvContractRTgt").addElement(gbpEpGroupFromProvContractRTgtSource).build());
    }

    /**
     * Retrieve an instance of EpGroupFromProvContractRTgt from the 
     * default managed object store by constructing its URI from the
     * path elements that lead to it.  If the object does not exist in
     * the local store, returns boost::none.  Note that even though it
     * may not exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpContract/[gbpContractName]/GbpEpGroupFromProvContractRTgt/[gbpEpGroupFromProvContractRTgtSource]
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpContractName the value of gbpContractName,
     * a naming property for Contract
     * @param gbpEpGroupFromProvContractRTgtSource the value of gbpEpGroupFromProvContractRTgtSource,
     * a naming property for EpGroupFromProvContractRTgt
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbp::EpGroupFromProvContractRTgt> > resolve(
        const std::string& policySpaceName,
        const std::string& gbpContractName,
        const std::string& gbpEpGroupFromProvContractRTgtSource)
    {
        return resolve(opflex::ofcore::OFFramework::defaultInstance(),policySpaceName,gbpContractName,gbpEpGroupFromProvContractRTgtSource);
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
     * Remove the EpGroupFromProvContractRTgt object with the specified URI
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
     * Remove the EpGroupFromProvContractRTgt object with the specified URI 
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
     * Remove the EpGroupFromProvContractRTgt object with the specified path
     * elements from the managed object store.  If the object does
     * not exist, then this will be a no-op.  If this object has any
     * children, they will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpContract/[gbpContractName]/GbpEpGroupFromProvContractRTgt/[gbpEpGroupFromProvContractRTgtSource]
     * 
     * @param framework the framework instance to use
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpContractName the value of gbpContractName,
     * a naming property for Contract
     * @param gbpEpGroupFromProvContractRTgtSource the value of gbpEpGroupFromProvContractRTgtSource,
     * a naming property for EpGroupFromProvContractRTgt
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        opflex::ofcore::OFFramework& framework,
        const std::string& policySpaceName,
        const std::string& gbpContractName,
        const std::string& gbpEpGroupFromProvContractRTgtSource)
    {
        MO::remove(framework, CLASS_ID, opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpContract").addElement(gbpContractName).addElement("GbpEpGroupFromProvContractRTgt").addElement(gbpEpGroupFromProvContractRTgtSource).build());
    }

    /**
     * Remove the EpGroupFromProvContractRTgt object with the specified path
     * elements from the managed object store using the default
     * framework instance.  If the object does not exist, then
     * this will be a no-op.  If this object has any children, they
     * will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpContract/[gbpContractName]/GbpEpGroupFromProvContractRTgt/[gbpEpGroupFromProvContractRTgtSource]
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpContractName the value of gbpContractName,
     * a naming property for Contract
     * @param gbpEpGroupFromProvContractRTgtSource the value of gbpEpGroupFromProvContractRTgtSource,
     * a naming property for EpGroupFromProvContractRTgt
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        const std::string& policySpaceName,
        const std::string& gbpContractName,
        const std::string& gbpEpGroupFromProvContractRTgtSource)
    {
        remove(opflex::ofcore::OFFramework::defaultInstance(),policySpaceName,gbpContractName,gbpEpGroupFromProvContractRTgtSource);
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
     * Construct an instance of EpGroupFromProvContractRTgt.
     * This should not typically be called from user code.
     */
    EpGroupFromProvContractRTgt(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri,
        const boost::shared_ptr<const opflex::modb::mointernal::ObjectInstance>& oi)
        : MO(framework, CLASS_ID, uri, oi) { }
}; // class EpGroupFromProvContractRTgt

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_EPGROUPFROMPROVCONTRACTRTGT_HPP
