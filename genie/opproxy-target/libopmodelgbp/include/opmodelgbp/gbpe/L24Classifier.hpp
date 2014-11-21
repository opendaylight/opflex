/**
 * 
 * SOME COPYRIGHT
 * 
 * L24Classifier.hpp
 * 
 * generated L24Classifier.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBPE_L24CLASSIFIER_HPP
#define GI_GBPE_L24CLASSIFIER_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(gbp/RuleFromClassifierRTgt)
 */
#include "opmodelgbp/gbp/RuleFromClassifierRTgt.hpp"

namespace opmodelgbp {
namespace gbpe {

class L24Classifier
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for L24Classifier
     */
    static const opflex::modb::class_id_t CLASS_ID = 26;

    /**
     * Check whether arpOpc has been set
     * @return true if arpOpc has been set
     */
    bool isArpOpcSet()
    {
        return getObjectInstance().isSet(851973ul, opflex::modb::PropertyInfo::ENUM8);
    }

    /**
     * Get the value of arpOpc if it has been set.
     * @return the value of arpOpc or boost::none if not set
     */
    boost::optional<const uint8_t> getArpOpc()
    {
        if (isArpOpcSet())
            return (const uint8_t)getObjectInstance().getUInt64(851973ul);
        return boost::none;
    }

    /**
     * Get the value of arpOpc if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of arpOpc if set, otherwise the value of default passed in
     */
    const uint8_t getArpOpc(const uint8_t defaultValue)
    {
        return getArpOpc().get_value_or(defaultValue);
    }

    /**
     * Set arpOpc to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& setArpOpc(const uint8_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(851973ul, newValue);
        return *this;
    }

    /**
     * Unset arpOpc in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& unsetArpOpc()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(851973ul, opflex::modb::PropertyInfo::ENUM8, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether connectionTracking has been set
     * @return true if connectionTracking has been set
     */
    bool isConnectionTrackingSet()
    {
        return getObjectInstance().isSet(851971ul, opflex::modb::PropertyInfo::ENUM8);
    }

    /**
     * Get the value of connectionTracking if it has been set.
     * @return the value of connectionTracking or boost::none if not set
     */
    boost::optional<const uint8_t> getConnectionTracking()
    {
        if (isConnectionTrackingSet())
            return (const uint8_t)getObjectInstance().getUInt64(851971ul);
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
    opmodelgbp::gbpe::L24Classifier& setConnectionTracking(const uint8_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(851971ul, newValue);
        return *this;
    }

    /**
     * Unset connectionTracking in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& unsetConnectionTracking()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(851971ul, opflex::modb::PropertyInfo::ENUM8, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether dFromPort has been set
     * @return true if dFromPort has been set
     */
    bool isDFromPortSet()
    {
        return getObjectInstance().isSet(851978ul, opflex::modb::PropertyInfo::U64);
    }

    /**
     * Get the value of dFromPort if it has been set.
     * @return the value of dFromPort or boost::none if not set
     */
    boost::optional<uint64_t> getDFromPort()
    {
        if (isDFromPortSet())
            return getObjectInstance().getUInt64(851978ul);
        return boost::none;
    }

    /**
     * Get the value of dFromPort if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of dFromPort if set, otherwise the value of default passed in
     */
    uint64_t getDFromPort(uint64_t defaultValue)
    {
        return getDFromPort().get_value_or(defaultValue);
    }

    /**
     * Set dFromPort to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& setDFromPort(uint64_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(851978ul, newValue);
        return *this;
    }

    /**
     * Unset dFromPort in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& unsetDFromPort()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(851978ul, opflex::modb::PropertyInfo::U64, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether dToPort has been set
     * @return true if dToPort has been set
     */
    bool isDToPortSet()
    {
        return getObjectInstance().isSet(851979ul, opflex::modb::PropertyInfo::U64);
    }

    /**
     * Get the value of dToPort if it has been set.
     * @return the value of dToPort or boost::none if not set
     */
    boost::optional<uint64_t> getDToPort()
    {
        if (isDToPortSet())
            return getObjectInstance().getUInt64(851979ul);
        return boost::none;
    }

    /**
     * Get the value of dToPort if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of dToPort if set, otherwise the value of default passed in
     */
    uint64_t getDToPort(uint64_t defaultValue)
    {
        return getDToPort().get_value_or(defaultValue);
    }

    /**
     * Set dToPort to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& setDToPort(uint64_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(851979ul, newValue);
        return *this;
    }

    /**
     * Unset dToPort in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& unsetDToPort()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(851979ul, opflex::modb::PropertyInfo::U64, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether direction has been set
     * @return true if direction has been set
     */
    bool isDirectionSet()
    {
        return getObjectInstance().isSet(851972ul, opflex::modb::PropertyInfo::ENUM8);
    }

    /**
     * Get the value of direction if it has been set.
     * @return the value of direction or boost::none if not set
     */
    boost::optional<const uint8_t> getDirection()
    {
        if (isDirectionSet())
            return (const uint8_t)getObjectInstance().getUInt64(851972ul);
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
    opmodelgbp::gbpe::L24Classifier& setDirection(const uint8_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(851972ul, newValue);
        return *this;
    }

    /**
     * Unset direction in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& unsetDirection()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(851972ul, opflex::modb::PropertyInfo::ENUM8, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether etherT has been set
     * @return true if etherT has been set
     */
    bool isEtherTSet()
    {
        return getObjectInstance().isSet(851974ul, opflex::modb::PropertyInfo::ENUM16);
    }

    /**
     * Get the value of etherT if it has been set.
     * @return the value of etherT or boost::none if not set
     */
    boost::optional<uint16_t> getEtherT()
    {
        if (isEtherTSet())
            return (uint16_t)getObjectInstance().getUInt64(851974ul);
        return boost::none;
    }

    /**
     * Get the value of etherT if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of etherT if set, otherwise the value of default passed in
     */
    uint16_t getEtherT(uint16_t defaultValue)
    {
        return getEtherT().get_value_or(defaultValue);
    }

    /**
     * Set etherT to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& setEtherT(uint16_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(851974ul, newValue);
        return *this;
    }

    /**
     * Unset etherT in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& unsetEtherT()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(851974ul, opflex::modb::PropertyInfo::ENUM16, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether name has been set
     * @return true if name has been set
     */
    bool isNameSet()
    {
        return getObjectInstance().isSet(851969ul, opflex::modb::PropertyInfo::STRING);
    }

    /**
     * Get the value of name if it has been set.
     * @return the value of name or boost::none if not set
     */
    boost::optional<const std::string&> getName()
    {
        if (isNameSet())
            return getObjectInstance().getString(851969ul);
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
    opmodelgbp::gbpe::L24Classifier& setName(const std::string& newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setString(851969ul, newValue);
        return *this;
    }

    /**
     * Unset name in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& unsetName()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(851969ul, opflex::modb::PropertyInfo::STRING, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether order has been set
     * @return true if order has been set
     */
    bool isOrderSet()
    {
        return getObjectInstance().isSet(851970ul, opflex::modb::PropertyInfo::U64);
    }

    /**
     * Get the value of order if it has been set.
     * @return the value of order or boost::none if not set
     */
    boost::optional<uint32_t> getOrder()
    {
        if (isOrderSet())
            return (uint32_t)getObjectInstance().getUInt64(851970ul);
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
    opmodelgbp::gbpe::L24Classifier& setOrder(uint32_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(851970ul, newValue);
        return *this;
    }

    /**
     * Unset order in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& unsetOrder()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(851970ul, opflex::modb::PropertyInfo::U64, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether prot has been set
     * @return true if prot has been set
     */
    bool isProtSet()
    {
        return getObjectInstance().isSet(851975ul, opflex::modb::PropertyInfo::U64);
    }

    /**
     * Get the value of prot if it has been set.
     * @return the value of prot or boost::none if not set
     */
    boost::optional<const uint8_t> getProt()
    {
        if (isProtSet())
            return (const uint8_t)getObjectInstance().getUInt64(851975ul);
        return boost::none;
    }

    /**
     * Get the value of prot if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of prot if set, otherwise the value of default passed in
     */
    const uint8_t getProt(const uint8_t defaultValue)
    {
        return getProt().get_value_or(defaultValue);
    }

    /**
     * Set prot to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& setProt(const uint8_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(851975ul, newValue);
        return *this;
    }

    /**
     * Unset prot in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& unsetProt()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(851975ul, opflex::modb::PropertyInfo::U64, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether sFromPort has been set
     * @return true if sFromPort has been set
     */
    bool isSFromPortSet()
    {
        return getObjectInstance().isSet(851976ul, opflex::modb::PropertyInfo::U64);
    }

    /**
     * Get the value of sFromPort if it has been set.
     * @return the value of sFromPort or boost::none if not set
     */
    boost::optional<uint64_t> getSFromPort()
    {
        if (isSFromPortSet())
            return getObjectInstance().getUInt64(851976ul);
        return boost::none;
    }

    /**
     * Get the value of sFromPort if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of sFromPort if set, otherwise the value of default passed in
     */
    uint64_t getSFromPort(uint64_t defaultValue)
    {
        return getSFromPort().get_value_or(defaultValue);
    }

    /**
     * Set sFromPort to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& setSFromPort(uint64_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(851976ul, newValue);
        return *this;
    }

    /**
     * Unset sFromPort in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& unsetSFromPort()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(851976ul, opflex::modb::PropertyInfo::U64, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether sToPort has been set
     * @return true if sToPort has been set
     */
    bool isSToPortSet()
    {
        return getObjectInstance().isSet(851977ul, opflex::modb::PropertyInfo::U64);
    }

    /**
     * Get the value of sToPort if it has been set.
     * @return the value of sToPort or boost::none if not set
     */
    boost::optional<uint64_t> getSToPort()
    {
        if (isSToPortSet())
            return getObjectInstance().getUInt64(851977ul);
        return boost::none;
    }

    /**
     * Get the value of sToPort if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of sToPort if set, otherwise the value of default passed in
     */
    uint64_t getSToPort(uint64_t defaultValue)
    {
        return getSToPort().get_value_or(defaultValue);
    }

    /**
     * Set sToPort to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& setSToPort(uint64_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(851977ul, newValue);
        return *this;
    }

    /**
     * Unset sToPort in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::L24Classifier& unsetSToPort()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(851977ul, opflex::modb::PropertyInfo::U64, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Retrieve an instance of L24Classifier from the managed
     * object store.  If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @param framework the framework instance to use
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbpe::L24Classifier> > resolve(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::gbpe::L24Classifier>(framework, CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of L24Classifier from the managed
     * object store using the default framework instance.  If the 
     * object does not exist in the local store, returns boost::none. 
     * Note that even though it may not exist locally, it may still 
     * exist remotely.
     * 
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbpe::L24Classifier> > resolve(
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::gbpe::L24Classifier>(opflex::ofcore::OFFramework::defaultInstance(), CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of L24Classifier from the managed
     * object store by constructing its URI from the path elements
     * that lead to it.  If the object does not exist in the local
     * store, returns boost::none.  Note that even though it may not
     * exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpeL24Classifier/[gbpeL24ClassifierName]
     * 
     * @param framework the framework instance to use 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpeL24ClassifierName the value of gbpeL24ClassifierName,
     * a naming property for L24Classifier
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbpe::L24Classifier> > resolve(
        opflex::ofcore::OFFramework& framework,
        const std::string& policySpaceName,
        const std::string& gbpeL24ClassifierName)
    {
        return resolve(framework,opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpeL24Classifier").addElement(gbpeL24ClassifierName).build());
    }

    /**
     * Retrieve an instance of L24Classifier from the 
     * default managed object store by constructing its URI from the
     * path elements that lead to it.  If the object does not exist in
     * the local store, returns boost::none.  Note that even though it
     * may not exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpeL24Classifier/[gbpeL24ClassifierName]
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpeL24ClassifierName the value of gbpeL24ClassifierName,
     * a naming property for L24Classifier
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbpe::L24Classifier> > resolve(
        const std::string& policySpaceName,
        const std::string& gbpeL24ClassifierName)
    {
        return resolve(opflex::ofcore::OFFramework::defaultInstance(),policySpaceName,gbpeL24ClassifierName);
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
            CLASS_ID, getURI(), 2148335651ul, 35,
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
            getFramework(), CLASS_ID, getURI(), 2148335651ul, 35, out);
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
     * Remove the L24Classifier object with the specified URI
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
     * Remove the L24Classifier object with the specified URI 
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
     * Remove the L24Classifier object with the specified path
     * elements from the managed object store.  If the object does
     * not exist, then this will be a no-op.  If this object has any
     * children, they will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpeL24Classifier/[gbpeL24ClassifierName]
     * 
     * @param framework the framework instance to use
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpeL24ClassifierName the value of gbpeL24ClassifierName,
     * a naming property for L24Classifier
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        opflex::ofcore::OFFramework& framework,
        const std::string& policySpaceName,
        const std::string& gbpeL24ClassifierName)
    {
        MO::remove(framework, CLASS_ID, opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpeL24Classifier").addElement(gbpeL24ClassifierName).build());
    }

    /**
     * Remove the L24Classifier object with the specified path
     * elements from the managed object store using the default
     * framework instance.  If the object does not exist, then
     * this will be a no-op.  If this object has any children, they
     * will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpeL24Classifier/[gbpeL24ClassifierName]
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpeL24ClassifierName the value of gbpeL24ClassifierName,
     * a naming property for L24Classifier
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        const std::string& policySpaceName,
        const std::string& gbpeL24ClassifierName)
    {
        remove(opflex::ofcore::OFFramework::defaultInstance(),policySpaceName,gbpeL24ClassifierName);
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
     * Construct an instance of L24Classifier.
     * This should not typically be called from user code.
     */
    L24Classifier(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri,
        const boost::shared_ptr<const opflex::modb::mointernal::ObjectInstance>& oi)
        : MO(framework, CLASS_ID, uri, oi) { }
}; // class L24Classifier

} // namespace gbpe
} // namespace opmodelgbp
#endif // GI_GBPE_L24CLASSIFIER_HPP
