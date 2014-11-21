/**
 * 
 * SOME COPYRIGHT
 * 
 * L2Ep.hpp
 * 
 * generated L2Ep.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_EPR_L2EP_HPP
#define GI_EPR_L2EP_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(epr/L3Net)
 */
#include "opmodelgbp/epr/L3Net.hpp"

namespace opmodelgbp {
namespace epr {

class L2Ep
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for L2Ep
     */
    static const opflex::modb::class_id_t CLASS_ID = 18;

    /**
     * Check whether context has been set
     * @return true if context has been set
     */
    bool isContextSet()
    {
        return getObjectInstance().isSet(589828ul, opflex::modb::PropertyInfo::STRING);
    }

    /**
     * Get the value of context if it has been set.
     * @return the value of context or boost::none if not set
     */
    boost::optional<const std::string&> getContext()
    {
        if (isContextSet())
            return getObjectInstance().getString(589828ul);
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
    opmodelgbp::epr::L2Ep& setContext(const std::string& newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setString(589828ul, newValue);
        return *this;
    }

    /**
     * Unset context in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epr::L2Ep& unsetContext()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(589828ul, opflex::modb::PropertyInfo::STRING, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether group has been set
     * @return true if group has been set
     */
    bool isGroupSet()
    {
        return getObjectInstance().isSet(589827ul, opflex::modb::PropertyInfo::STRING);
    }

    /**
     * Get the value of group if it has been set.
     * @return the value of group or boost::none if not set
     */
    boost::optional<const std::string&> getGroup()
    {
        if (isGroupSet())
            return getObjectInstance().getString(589827ul);
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
    opmodelgbp::epr::L2Ep& setGroup(const std::string& newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setString(589827ul, newValue);
        return *this;
    }

    /**
     * Unset group in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epr::L2Ep& unsetGroup()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(589827ul, opflex::modb::PropertyInfo::STRING, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether mac has been set
     * @return true if mac has been set
     */
    bool isMacSet()
    {
        return getObjectInstance().isSet(589826ul, opflex::modb::PropertyInfo::MAC);
    }

    /**
     * Get the value of mac if it has been set.
     * @return the value of mac or boost::none if not set
     */
    boost::optional<const opflex::modb::MAC&> getMac()
    {
        if (isMacSet())
            return getObjectInstance().getMAC(589826ul);
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
    opmodelgbp::epr::L2Ep& setMac(const opflex::modb::MAC& newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setMAC(589826ul, newValue);
        return *this;
    }

    /**
     * Unset mac in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epr::L2Ep& unsetMac()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(589826ul, opflex::modb::PropertyInfo::MAC, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether uuid has been set
     * @return true if uuid has been set
     */
    bool isUuidSet()
    {
        return getObjectInstance().isSet(589825ul, opflex::modb::PropertyInfo::STRING);
    }

    /**
     * Get the value of uuid if it has been set.
     * @return the value of uuid or boost::none if not set
     */
    boost::optional<const std::string&> getUuid()
    {
        if (isUuidSet())
            return getObjectInstance().getString(589825ul);
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
    opmodelgbp::epr::L2Ep& setUuid(const std::string& newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setString(589825ul, newValue);
        return *this;
    }

    /**
     * Unset uuid in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epr::L2Ep& unsetUuid()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(589825ul, opflex::modb::PropertyInfo::STRING, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Retrieve an instance of L2Ep from the managed
     * object store.  If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @param framework the framework instance to use
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::epr::L2Ep> > resolve(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::epr::L2Ep>(framework, CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of L2Ep from the managed
     * object store using the default framework instance.  If the 
     * object does not exist in the local store, returns boost::none. 
     * Note that even though it may not exist locally, it may still 
     * exist remotely.
     * 
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::epr::L2Ep> > resolve(
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::epr::L2Ep>(opflex::ofcore::OFFramework::defaultInstance(), CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of L2Ep from the managed
     * object store by constructing its URI from the path elements
     * that lead to it.  If the object does not exist in the local
     * store, returns boost::none.  Note that even though it may not
     * exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /EprL2Universe/EprL2Ep/[eprL2EpContext]/[eprL2EpMac]
     * 
     * @param framework the framework instance to use 
     * @param eprL2EpContext the value of eprL2EpContext,
     * a naming property for L2Ep
     * @param eprL2EpMac the value of eprL2EpMac,
     * a naming property for L2Ep
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::epr::L2Ep> > resolve(
        opflex::ofcore::OFFramework& framework,
        const std::string& eprL2EpContext,
        const opflex::modb::MAC& eprL2EpMac)
    {
        return resolve(framework,opflex::modb::URIBuilder().addElement("EprL2Universe").addElement("EprL2Ep").addElement(eprL2EpContext).addElement(eprL2EpMac).build());
    }

    /**
     * Retrieve an instance of L2Ep from the 
     * default managed object store by constructing its URI from the
     * path elements that lead to it.  If the object does not exist in
     * the local store, returns boost::none.  Note that even though it
     * may not exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /EprL2Universe/EprL2Ep/[eprL2EpContext]/[eprL2EpMac]
     * 
     * @param eprL2EpContext the value of eprL2EpContext,
     * a naming property for L2Ep
     * @param eprL2EpMac the value of eprL2EpMac,
     * a naming property for L2Ep
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::epr::L2Ep> > resolve(
        const std::string& eprL2EpContext,
        const opflex::modb::MAC& eprL2EpMac)
    {
        return resolve(opflex::ofcore::OFFramework::defaultInstance(),eprL2EpContext,eprL2EpMac);
    }

    /**
     * Retrieve the child object with the specified naming
     * properties. If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @param eprL3NetIp the value of eprL3NetIp,
     * a naming property for L3Net
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    boost::optional<boost::shared_ptr<opmodelgbp::epr::L3Net> > resolveEprL3Net(
        const std::string& eprL3NetIp)
    {
        return opmodelgbp::epr::L3Net::resolve(getFramework(), opflex::modb::URIBuilder(getURI()).addElement("EprL3Net").addElement(eprL3NetIp).build());
    }

    /**
     * Create a new child object with the specified naming properties
     * and make it a child of this object in the currently-active
     * mutator.  If the object already exists in the store, get a
     * mutable copy of that object.  If the object already exists in
     * the mutator, get a reference to the object.
     * 
     * @param eprL3NetIp the value of eprL3NetIp,
     * a naming property for L3Net
     * @throws std::logic_error if no mutator is active
     * @return a shared pointer to the (possibly new) object
     */
    boost::shared_ptr<opmodelgbp::epr::L3Net> addEprL3Net(
        const std::string& eprL3NetIp)
    {
        boost::shared_ptr<opmodelgbp::epr::L3Net> result = addChild<opmodelgbp::epr::L3Net>(
            CLASS_ID, getURI(), 2148073491ul, 19,
            opflex::modb::URIBuilder(getURI()).addElement("EprL3Net").addElement(eprL3NetIp).build()
            );
        result->setIp(eprL3NetIp);
        return result;
    }

    /**
     * Resolve and retrieve all of the immediate children of type
     * opmodelgbp::epr::L3Net
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
    void resolveEprL3Net(/* out */ std::vector<boost::shared_ptr<opmodelgbp::epr::L3Net> >& out)
    {
        opflex::modb::mointernal::MO::resolveChildren<opmodelgbp::epr::L3Net>(
            getFramework(), CLASS_ID, getURI(), 2148073491ul, 19, out);
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
     * Remove the L2Ep object with the specified URI
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
     * Remove the L2Ep object with the specified URI 
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
     * Remove the L2Ep object with the specified path
     * elements from the managed object store.  If the object does
     * not exist, then this will be a no-op.  If this object has any
     * children, they will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /EprL2Universe/EprL2Ep/[eprL2EpContext]/[eprL2EpMac]
     * 
     * @param framework the framework instance to use
     * @param eprL2EpContext the value of eprL2EpContext,
     * a naming property for L2Ep
     * @param eprL2EpMac the value of eprL2EpMac,
     * a naming property for L2Ep
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        opflex::ofcore::OFFramework& framework,
        const std::string& eprL2EpContext,
        const opflex::modb::MAC& eprL2EpMac)
    {
        MO::remove(framework, CLASS_ID, opflex::modb::URIBuilder().addElement("EprL2Universe").addElement("EprL2Ep").addElement(eprL2EpContext).addElement(eprL2EpMac).build());
    }

    /**
     * Remove the L2Ep object with the specified path
     * elements from the managed object store using the default
     * framework instance.  If the object does not exist, then
     * this will be a no-op.  If this object has any children, they
     * will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /EprL2Universe/EprL2Ep/[eprL2EpContext]/[eprL2EpMac]
     * 
     * @param eprL2EpContext the value of eprL2EpContext,
     * a naming property for L2Ep
     * @param eprL2EpMac the value of eprL2EpMac,
     * a naming property for L2Ep
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        const std::string& eprL2EpContext,
        const opflex::modb::MAC& eprL2EpMac)
    {
        remove(opflex::ofcore::OFFramework::defaultInstance(),eprL2EpContext,eprL2EpMac);
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
     * Construct an instance of L2Ep.
     * This should not typically be called from user code.
     */
    L2Ep(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri,
        const boost::shared_ptr<const opflex::modb::mointernal::ObjectInstance>& oi)
        : MO(framework, CLASS_ID, uri, oi) { }
}; // class L2Ep

} // namespace epr
} // namespace opmodelgbp
#endif // GI_EPR_L2EP_HPP
