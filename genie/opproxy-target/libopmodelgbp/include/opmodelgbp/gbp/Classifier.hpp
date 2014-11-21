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
    static const opflex::modb::class_id_t CLASS_ID = 30;

    /**
     * Check whether connectionTracking has been set
     * @return true if connectionTracking has been set
     */
    bool isConnectionTrackingSet()
    {
        return isPropertySet(983043ul;
    }

    /**
     * Get the value of connectionTracking if it has been set.
     * @return the value of connectionTracking or boost::none if not set
     */
    boost::optional<const uint8_t> getConnectionTracking()
    {
        if (isConnectionTrackingSet())
            return (const uint8_t)getProperty(983043ul);
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
    opmodelgbp::gbp::Classifier& setConnectionTracking(const uint8_t newValue)
    {
        setProperty(983043ul, newValue);
        return *this;
    }

    /**
     * Check whether direction has been set
     * @return true if direction has been set
     */
    bool isDirectionSet()
    {
        return isPropertySet(983044ul;
    }

    /**
     * Get the value of direction if it has been set.
     * @return the value of direction or boost::none if not set
     */
    boost::optional<const uint8_t> getDirection()
    {
        if (isDirectionSet())
            return (const uint8_t)getProperty(983044ul);
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
    opmodelgbp::gbp::Classifier& setDirection(const uint8_t newValue)
    {
        setProperty(983044ul, newValue);
        return *this;
    }

    /**
     * Check whether name has been set
     * @return true if name has been set
     */
    bool isNameSet()
    {
        return isPropertySet(983041ul;
    }

    /**
     * Get the value of name if it has been set.
     * @return the value of name or boost::none if not set
     */
    boost::optional<const std::string&> getName()
    {
        if (isNameSet())
            return getProperty(983041ul);
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
    opmodelgbp::gbp::Classifier& setName(const std::string& newValue)
    {
        setProperty(983041ul, newValue);
        return *this;
    }

    /**
     * Check whether order has been set
     * @return true if order has been set
     */
    bool isOrderSet()
    {
        return isPropertySet(983042ul;
    }

    /**
     * Get the value of order if it has been set.
     * @return the value of order or boost::none if not set
     */
    boost::optional<uint32_t> getOrder()
    {
        if (isOrderSet())
            return (uint32_t)getProperty(983042ul);
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
    opmodelgbp::gbp::Classifier& setOrder(uint32_t newValue)
    {
        setProperty(983042ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of Classifier.
     * This should not typically be called from user code.
     */
    Classifier(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class Classifier

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_CLASSIFIER_HPP
