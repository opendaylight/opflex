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
    static const opflex::modb::class_id_t CLASS_ID = 14;

    /**
     * Check whether arpOpc has been set
     * @return true if arpOpc has been set
     */
    bool isArpOpcSet()
    {
        return isPropertySet(458757ul;
    }

    /**
     * Get the value of arpOpc if it has been set.
     * @return the value of arpOpc or boost::none if not set
     */
    boost::optional<const uint8_t> getArpOpc()
    {
        if (isArpOpcSet())
            return (const uint8_t)getProperty(458757ul);
        return boost::none;
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
        setProperty(458757ul, newValue);
        return *this;
    }

    /**
     * Check whether connectionTracking has been set
     * @return true if connectionTracking has been set
     */
    bool isConnectionTrackingSet()
    {
        return isPropertySet(458755ul;
    }

    /**
     * Get the value of connectionTracking if it has been set.
     * @return the value of connectionTracking or boost::none if not set
     */
    boost::optional<const uint8_t> getConnectionTracking()
    {
        if (isConnectionTrackingSet())
            return (const uint8_t)getProperty(458755ul);
        return boost::none;
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
        setProperty(458755ul, newValue);
        return *this;
    }

    /**
     * Check whether dFromPort has been set
     * @return true if dFromPort has been set
     */
    bool isDFromPortSet()
    {
        return isPropertySet(458762ul;
    }

    /**
     * Get the value of dFromPort if it has been set.
     * @return the value of dFromPort or boost::none if not set
     */
    boost::optional<uint64_t> getDFromPort()
    {
        if (isDFromPortSet())
            return getProperty(458762ul);
        return boost::none;
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
        setProperty(458762ul, newValue);
        return *this;
    }

    /**
     * Check whether dToPort has been set
     * @return true if dToPort has been set
     */
    bool isDToPortSet()
    {
        return isPropertySet(458763ul;
    }

    /**
     * Get the value of dToPort if it has been set.
     * @return the value of dToPort or boost::none if not set
     */
    boost::optional<uint64_t> getDToPort()
    {
        if (isDToPortSet())
            return getProperty(458763ul);
        return boost::none;
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
        setProperty(458763ul, newValue);
        return *this;
    }

    /**
     * Check whether direction has been set
     * @return true if direction has been set
     */
    bool isDirectionSet()
    {
        return isPropertySet(458756ul;
    }

    /**
     * Get the value of direction if it has been set.
     * @return the value of direction or boost::none if not set
     */
    boost::optional<const uint8_t> getDirection()
    {
        if (isDirectionSet())
            return (const uint8_t)getProperty(458756ul);
        return boost::none;
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
        setProperty(458756ul, newValue);
        return *this;
    }

    /**
     * Check whether etherT has been set
     * @return true if etherT has been set
     */
    bool isEtherTSet()
    {
        return isPropertySet(458758ul;
    }

    /**
     * Get the value of etherT if it has been set.
     * @return the value of etherT or boost::none if not set
     */
    boost::optional<uint16_t> getEtherT()
    {
        if (isEtherTSet())
            return (uint16_t)getProperty(458758ul);
        return boost::none;
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
        setProperty(458758ul, newValue);
        return *this;
    }

    /**
     * Check whether name has been set
     * @return true if name has been set
     */
    bool isNameSet()
    {
        return isPropertySet(458753ul;
    }

    /**
     * Get the value of name if it has been set.
     * @return the value of name or boost::none if not set
     */
    boost::optional<const std::string&> getName()
    {
        if (isNameSet())
            return getProperty(458753ul);
        return boost::none;
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
        setProperty(458753ul, newValue);
        return *this;
    }

    /**
     * Check whether order has been set
     * @return true if order has been set
     */
    bool isOrderSet()
    {
        return isPropertySet(458754ul;
    }

    /**
     * Get the value of order if it has been set.
     * @return the value of order or boost::none if not set
     */
    boost::optional<uint32_t> getOrder()
    {
        if (isOrderSet())
            return (uint32_t)getProperty(458754ul);
        return boost::none;
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
        setProperty(458754ul, newValue);
        return *this;
    }

    /**
     * Check whether prot has been set
     * @return true if prot has been set
     */
    bool isProtSet()
    {
        return isPropertySet(458759ul;
    }

    /**
     * Get the value of prot if it has been set.
     * @return the value of prot or boost::none if not set
     */
    boost::optional<const uint8_t> getProt()
    {
        if (isProtSet())
            return (const uint8_t)getProperty(458759ul);
        return boost::none;
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
        setProperty(458759ul, newValue);
        return *this;
    }

    /**
     * Check whether sFromPort has been set
     * @return true if sFromPort has been set
     */
    bool isSFromPortSet()
    {
        return isPropertySet(458760ul;
    }

    /**
     * Get the value of sFromPort if it has been set.
     * @return the value of sFromPort or boost::none if not set
     */
    boost::optional<uint64_t> getSFromPort()
    {
        if (isSFromPortSet())
            return getProperty(458760ul);
        return boost::none;
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
        setProperty(458760ul, newValue);
        return *this;
    }

    /**
     * Check whether sToPort has been set
     * @return true if sToPort has been set
     */
    bool isSToPortSet()
    {
        return isPropertySet(458761ul;
    }

    /**
     * Get the value of sToPort if it has been set.
     * @return the value of sToPort or boost::none if not set
     */
    boost::optional<uint64_t> getSToPort()
    {
        if (isSToPortSet())
            return getProperty(458761ul);
        return boost::none;
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
        setProperty(458761ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of L24Classifier.
     * This should not typically be called from user code.
     */
    L24Classifier(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class L24Classifier

} // namespace gbpe
} // namespace opmodelgbp
#endif // GI_GBPE_L24CLASSIFIER_HPP
