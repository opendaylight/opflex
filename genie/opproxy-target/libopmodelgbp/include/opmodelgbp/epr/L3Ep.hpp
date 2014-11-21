/**
 * 
 * SOME COPYRIGHT
 * 
 * L3Ep.hpp
 * 
 * generated L3Ep.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_EPR_L3EP_HPP
#define GI_EPR_L3EP_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace epr {

class L3Ep
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for L3Ep
     */
    static const opflex::modb::class_id_t CLASS_ID = 20;

    /**
     * Check whether context has been set
     * @return true if context has been set
     */
    bool isContextSet()
    {
        return getObjectInstance().isSet(655365ul, opflex::modb::PropertyInfo::STRING);
    }

    /**
     * Get the value of context if it has been set.
     * @return the value of context or boost::none if not set
     */
    boost::optional<const std::string&> getContext()
    {
        if (isContextSet())
            return getObjectInstance().getString(655365ul);
        return boost::none;
    }

    /**
     * Get the value of context if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of context if set, otherwise the value of default passed in
     */
    const std::string& getContext(const std::string& defaultValue)
    {
        return getContext().get_value_or(defaultValue);
    }

    /**
     * Set context to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epr::L3Ep& setContext(const std::string& newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setString(655365ul, newValue);
        return *this;
    }

    /**
     * Unset context in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epr::L3Ep& unsetContext()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(655365ul, opflex::modb::PropertyInfo::STRING, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether group has been set
     * @return true if group has been set
     */
    bool isGroupSet()
    {
        return getObjectInstance().isSet(655363ul, opflex::modb::PropertyInfo::STRING);
    }

    /**
     * Get the value of group if it has been set.
     * @return the value of group or boost::none if not set
     */
    boost::optional<const std::string&> getGroup()
    {
        if (isGroupSet())
            return getObjectInstance().getString(655363ul);
        return boost::none;
    }

    /**
     * Get the value of group if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of group if set, otherwise the value of default passed in
     */
    const std::string& getGroup(const std::string& defaultValue)
    {
        return getGroup().get_value_or(defaultValue);
    }

    /**
     * Set group to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epr::L3Ep& setGroup(const std::string& newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setString(655363ul, newValue);
        return *this;
    }

    /**
     * Unset group in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epr::L3Ep& unsetGroup()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(655363ul, opflex::modb::PropertyInfo::STRING, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether ip has been set
     * @return true if ip has been set
     */
    bool isIpSet()
    {
        return getObjectInstance().isSet(655364ul, opflex::modb::PropertyInfo::STRING);
    }

    /**
     * Get the value of ip if it has been set.
     * @return the value of ip or boost::none if not set
     */
    boost::optional<const std::string&> getIp()
    {
        if (isIpSet())
            return getObjectInstance().getString(655364ul);
        return boost::none;
    }

    /**
     * Get the value of ip if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of ip if set, otherwise the value of default passed in
     */
    const std::string& getIp(const std::string& defaultValue)
    {
        return getIp().get_value_or(defaultValue);
    }

    /**
     * Set ip to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epr::L3Ep& setIp(const std::string& newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setString(655364ul, newValue);
        return *this;
    }

    /**
     * Unset ip in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epr::L3Ep& unsetIp()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(655364ul, opflex::modb::PropertyInfo::STRING, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether mac has been set
     * @return true if mac has been set
     */
    bool isMacSet()
    {
        return getObjectInstance().isSet(655362ul, opflex::modb::PropertyInfo::MAC);
    }

    /**
     * Get the value of mac if it has been set.
     * @return the value of mac or boost::none if not set
     */
    boost::optional<const opflex::modb::MAC&> getMac()
    {
        if (isMacSet())
            return getObjectInstance().getMAC(655362ul);
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
    opmodelgbp::epr::L3Ep& setMac(const opflex::modb::MAC& newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setMAC(655362ul, newValue);
        return *this;
    }

    /**
     * Unset mac in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epr::L3Ep& unsetMac()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(655362ul, opflex::modb::PropertyInfo::MAC, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether uuid has been set
     * @return true if uuid has been set
     */
    bool isUuidSet()
    {
        return getObjectInstance().isSet(655361ul, opflex::modb::PropertyInfo::STRING);
    }

    /**
     * Get the value of uuid if it has been set.
     * @return the value of uuid or boost::none if not set
     */
    boost::optional<const std::string&> getUuid()
    {
        if (isUuidSet())
            return getObjectInstance().getString(655361ul);
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
    opmodelgbp::epr::L3Ep& setUuid(const std::string& newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setString(655361ul, newValue);
        return *this;
    }

    /**
     * Unset uuid in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epr::L3Ep& unsetUuid()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(655361ul, opflex::modb::PropertyInfo::STRING, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Retrieve an instance of L3Ep from the managed
     * object store.  If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @param framework the framework instance to use
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::epr::L3Ep> > resolve(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::epr::L3Ep>(framework, CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of L3Ep from the managed
     * object store using the default framework instance.  If the 
     * object does not exist in the local store, returns boost::none. 
     * Note that even though it may not exist locally, it may still 
     * exist remotely.
     * 
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::epr::L3Ep> > resolve(
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::epr::L3Ep>(opflex::ofcore::OFFramework::defaultInstance(), CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of L3Ep from the managed
     * object store by constructing its URI from the path elements
     * that lead to it.  If the object does not exist in the local
     * store, returns boost::none.  Note that even though it may not
     * exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /EprL3Universe/EprL3Ep/[eprL3EpContext]/[eprL3EpIp]
     * 
     * @param framework the framework instance to use 
     * @param eprL3EpContext the value of eprL3EpContext,
     * a naming property for L3Ep
     * @param eprL3EpIp the value of eprL3EpIp,
     * a naming property for L3Ep
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::epr::L3Ep> > resolve(
        opflex::ofcore::OFFramework& framework,
        const std::string& eprL3EpContext,
        const std::string& eprL3EpIp)
    {
        return resolve(framework,opflex::modb::URIBuilder().addElement("EprL3Universe").addElement("EprL3Ep").addElement(eprL3EpContext).addElement(eprL3EpIp).build());
    }

    /**
     * Retrieve an instance of L3Ep from the 
     * default managed object store by constructing its URI from the
     * path elements that lead to it.  If the object does not exist in
     * the local store, returns boost::none.  Note that even though it
     * may not exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /EprL3Universe/EprL3Ep/[eprL3EpContext]/[eprL3EpIp]
     * 
     * @param eprL3EpContext the value of eprL3EpContext,
     * a naming property for L3Ep
     * @param eprL3EpIp the value of eprL3EpIp,
     * a naming property for L3Ep
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::epr::L3Ep> > resolve(
        const std::string& eprL3EpContext,
        const std::string& eprL3EpIp)
    {
        return resolve(opflex::ofcore::OFFramework::defaultInstance(),eprL3EpContext,eprL3EpIp);
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
     * Remove the L3Ep object with the specified URI
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
     * Remove the L3Ep object with the specified URI 
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
     * Remove the L3Ep object with the specified path
     * elements from the managed object store.  If the object does
     * not exist, then this will be a no-op.  If this object has any
     * children, they will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /EprL3Universe/EprL3Ep/[eprL3EpContext]/[eprL3EpIp]
     * 
     * @param framework the framework instance to use
     * @param eprL3EpContext the value of eprL3EpContext,
     * a naming property for L3Ep
     * @param eprL3EpIp the value of eprL3EpIp,
     * a naming property for L3Ep
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        opflex::ofcore::OFFramework& framework,
        const std::string& eprL3EpContext,
        const std::string& eprL3EpIp)
    {
        MO::remove(framework, CLASS_ID, opflex::modb::URIBuilder().addElement("EprL3Universe").addElement("EprL3Ep").addElement(eprL3EpContext).addElement(eprL3EpIp).build());
    }

    /**
     * Remove the L3Ep object with the specified path
     * elements from the managed object store using the default
     * framework instance.  If the object does not exist, then
     * this will be a no-op.  If this object has any children, they
     * will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /EprL3Universe/EprL3Ep/[eprL3EpContext]/[eprL3EpIp]
     * 
     * @param eprL3EpContext the value of eprL3EpContext,
     * a naming property for L3Ep
     * @param eprL3EpIp the value of eprL3EpIp,
     * a naming property for L3Ep
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        const std::string& eprL3EpContext,
        const std::string& eprL3EpIp)
    {
        remove(opflex::ofcore::OFFramework::defaultInstance(),eprL3EpContext,eprL3EpIp);
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
     * Construct an instance of L3Ep.
     * This should not typically be called from user code.
     */
    L3Ep(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri,
        const boost::shared_ptr<const opflex::modb::mointernal::ObjectInstance>& oi)
        : MO(framework, CLASS_ID, uri, oi) { }
}; // class L3Ep

} // namespace epr
} // namespace opmodelgbp
#endif // GI_EPR_L3EP_HPP
