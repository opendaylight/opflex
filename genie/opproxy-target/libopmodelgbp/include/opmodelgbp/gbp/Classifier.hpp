/**
 * 
 * SOME COPYRIGHT
 * 
 * Classifier.hpp
 * 
 * generated Classifier.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_CLASSIFIER_HPP
#define GI_GBP_CLASSIFIER_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(gbp/RuleFromClassifierRTgt)
 */
#include "opmodelgbp/gbp/RuleFromClassifierRTgt.hpp"

namespace opmodelgbp {
namespace gbp {

class Classifier
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Classifier
     */
    static const opflex::modb::class_id_t CLASS_ID = 21;

    /**
     * Check whether connectionTracking has been set
     * @return true if connectionTracking has been set
     */
    bool isConnectionTrackingSet()
    {
        return getObjectInstance().isSet(688131ul, opflex::modb::PropertyInfo::ENUM8);
    }

    /**
     * Get the value of connectionTracking if it has been set.
     * @return the value of connectionTracking or boost::none if not set
     */
    boost::optional<const uint8_t> getConnectionTracking()
    {
        if (isConnectionTrackingSet())
            return (const uint8_t)getObjectInstance().getUInt64(688131ul);
        return boost::none;
    }

    /**
     * Get the value of connectionTracking if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of connectionTracking if set, otherwise the value of default passed in
     */
    const uint8_t getConnectionTracking(const uint8_t defaultValue)
    {
        return getConnectionTracking().get_value_or(defaultValue);
    }

    /**
     * Set connectionTracking to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::Classifier& setConnectionTracking(const uint8_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(688131ul, newValue);
        return *this;
    }

    /**
     * Unset connectionTracking in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::Classifier& unsetConnectionTracking()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(688131ul, opflex::modb::PropertyInfo::ENUM8, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether direction has been set
     * @return true if direction has been set
     */
    bool isDirectionSet()
    {
        return getObjectInstance().isSet(688132ul, opflex::modb::PropertyInfo::ENUM8);
    }

    /**
     * Get the value of direction if it has been set.
     * @return the value of direction or boost::none if not set
     */
    boost::optional<const uint8_t> getDirection()
    {
        if (isDirectionSet())
            return (const uint8_t)getObjectInstance().getUInt64(688132ul);
        return boost::none;
    }

    /**
     * Get the value of direction if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of direction if set, otherwise the value of default passed in
     */
    const uint8_t getDirection(const uint8_t defaultValue)
    {
        return getDirection().get_value_or(defaultValue);
    }

    /**
     * Set direction to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::Classifier& setDirection(const uint8_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(688132ul, newValue);
        return *this;
    }

    /**
     * Unset direction in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::Classifier& unsetDirection()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(688132ul, opflex::modb::PropertyInfo::ENUM8, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether name has been set
     * @return true if name has been set
     */
    bool isNameSet()
    {
        return getObjectInstance().isSet(688129ul, opflex::modb::PropertyInfo::STRING);
    }

    /**
     * Get the value of name if it has been set.
     * @return the value of name or boost::none if not set
     */
    boost::optional<const std::string&> getName()
    {
        if (isNameSet())
            return getObjectInstance().getString(688129ul);
        return boost::none;
    }

    /**
     * Get the value of name if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of name if set, otherwise the value of default passed in
     */
    const std::string& getName(const std::string& defaultValue)
    {
        return getName().get_value_or(defaultValue);
    }

    /**
     * Set name to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::Classifier& setName(const std::string& newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setString(688129ul, newValue);
        return *this;
    }

    /**
     * Unset name in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::Classifier& unsetName()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(688129ul, opflex::modb::PropertyInfo::STRING, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether order has been set
     * @return true if order has been set
     */
    bool isOrderSet()
    {
        return getObjectInstance().isSet(688130ul, opflex::modb::PropertyInfo::U64);
    }

    /**
     * Get the value of order if it has been set.
     * @return the value of order or boost::none if not set
     */
    boost::optional<uint32_t> getOrder()
    {
        if (isOrderSet())
            return (uint32_t)getObjectInstance().getUInt64(688130ul);
        return boost::none;
    }

    /**
     * Get the value of order if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of order if set, otherwise the value of default passed in
     */
    uint32_t getOrder(uint32_t defaultValue)
    {
        return getOrder().get_value_or(defaultValue);
    }

    /**
     * Set order to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::Classifier& setOrder(uint32_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(688130ul, newValue);
        return *this;
    }

    /**
     * Unset order in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::Classifier& unsetOrder()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(688130ul, opflex::modb::PropertyInfo::U64, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Retrieve an instance of Classifier from the managed
     * object store.  If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @param framework the framework instance to use
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbp::Classifier> > resolve(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::gbp::Classifier>(framework, CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of Classifier from the managed
     * object store using the default framework instance.  If the 
     * object does not exist in the local store, returns boost::none. 
     * Note that even though it may not exist locally, it may still 
     * exist remotely.
     * 
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbp::Classifier> > resolve(
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::gbp::Classifier>(opflex::ofcore::OFFramework::defaultInstance(), CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of Classifier from the managed
     * object store by constructing its URI from the path elements
     * that lead to it.  If the object does not exist in the local
     * store, returns boost::none.  Note that even though it may not
     * exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpClassifier/[gbpClassifierName]
     * 
     * @param framework the framework instance to use 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpClassifierName the value of gbpClassifierName,
     * a naming property for Classifier
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbp::Classifier> > resolve(
        opflex::ofcore::OFFramework& framework,
        const std::string& policySpaceName,
        const std::string& gbpClassifierName)
    {
        return resolve(framework,opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpClassifier").addElement(gbpClassifierName).build());
    }

    /**
     * Retrieve an instance of Classifier from the 
     * default managed object store by constructing its URI from the
     * path elements that lead to it.  If the object does not exist in
     * the local store, returns boost::none.  Note that even though it
     * may not exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpClassifier/[gbpClassifierName]
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpClassifierName the value of gbpClassifierName,
     * a naming property for Classifier
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbp::Classifier> > resolve(
        const std::string& policySpaceName,
        const std::string& gbpClassifierName)
    {
        return resolve(opflex::ofcore::OFFramework::defaultInstance(),policySpaceName,gbpClassifierName);
    }

    /**
     * Retrieve the child object with the specified naming
     * properties. If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @param gbpRuleFromClassifierRTgtSource the value of gbpRuleFromClassifierRTgtSource,
     * a naming property for RuleFromClassifierRTgt
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    boost::optional<boost::shared_ptr<opmodelgbp::gbp::RuleFromClassifierRTgt> > resolveGbpRuleFromClassifierRTgt(
        const std::string& gbpRuleFromClassifierRTgtSource)
    {
        return opmodelgbp::gbp::RuleFromClassifierRTgt::resolve(getFramework(), opflex::modb::URIBuilder(getURI()).addElement("GbpRuleFromClassifierRTgt").addElement(gbpRuleFromClassifierRTgtSource).build());
    }

    /**
     * Create a new child object with the specified naming properties
     * and make it a child of this object in the currently-active
     * mutator.  If the object already exists in the store, get a
     * mutable copy of that object.  If the object already exists in
     * the mutator, get a reference to the object.
     * 
     * @param gbpRuleFromClassifierRTgtSource the value of gbpRuleFromClassifierRTgtSource,
     * a naming property for RuleFromClassifierRTgt
     * @throws std::logic_error if no mutator is active
     * @return a shared pointer to the (possibly new) object
     */
    boost::shared_ptr<opmodelgbp::gbp::RuleFromClassifierRTgt> addGbpRuleFromClassifierRTgt(
        const std::string& gbpRuleFromClassifierRTgtSource)
    {
        boost::shared_ptr<opmodelgbp::gbp::RuleFromClassifierRTgt> result = addChild<opmodelgbp::gbp::RuleFromClassifierRTgt>(
            CLASS_ID, getURI(), 2148171811ul, 35,
            opflex::modb::URIBuilder(getURI()).addElement("GbpRuleFromClassifierRTgt").addElement(gbpRuleFromClassifierRTgtSource).build()
            );
        result->setSource(gbpRuleFromClassifierRTgtSource);
        return result;
    }

    /**
     * Resolve and retrieve all of the immediate children of type
     * opmodelgbp::gbp::RuleFromClassifierRTgt
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
    void resolveGbpRuleFromClassifierRTgt(/* out */ std::vector<boost::shared_ptr<opmodelgbp::gbp::RuleFromClassifierRTgt> >& out)
    {
        opflex::modb::mointernal::MO::resolveChildren<opmodelgbp::gbp::RuleFromClassifierRTgt>(
            getFramework(), CLASS_ID, getURI(), 2148171811ul, 35, out);
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
     * Remove the Classifier object with the specified URI
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
     * Remove the Classifier object with the specified URI 
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
     * Remove the Classifier object with the specified path
     * elements from the managed object store.  If the object does
     * not exist, then this will be a no-op.  If this object has any
     * children, they will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpClassifier/[gbpClassifierName]
     * 
     * @param framework the framework instance to use
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpClassifierName the value of gbpClassifierName,
     * a naming property for Classifier
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        opflex::ofcore::OFFramework& framework,
        const std::string& policySpaceName,
        const std::string& gbpClassifierName)
    {
        MO::remove(framework, CLASS_ID, opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpClassifier").addElement(gbpClassifierName).build());
    }

    /**
     * Remove the Classifier object with the specified path
     * elements from the managed object store using the default
     * framework instance.  If the object does not exist, then
     * this will be a no-op.  If this object has any children, they
     * will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpClassifier/[gbpClassifierName]
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpClassifierName the value of gbpClassifierName,
     * a naming property for Classifier
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        const std::string& policySpaceName,
        const std::string& gbpClassifierName)
    {
        remove(opflex::ofcore::OFFramework::defaultInstance(),policySpaceName,gbpClassifierName);
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
     * Construct an instance of Classifier.
     * This should not typically be called from user code.
     */
    Classifier(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri,
        const boost::shared_ptr<const opflex::modb::mointernal::ObjectInstance>& oi)
        : MO(framework, CLASS_ID, uri, oi) { }
}; // class Classifier

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_CLASSIFIER_HPP
