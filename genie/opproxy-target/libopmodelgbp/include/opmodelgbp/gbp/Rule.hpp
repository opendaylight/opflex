/**
 * 
 * SOME COPYRIGHT
 * 
 * Rule.hpp
 * 
 * generated Rule.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_RULE_HPP
#define GI_GBP_RULE_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(gbp/RuleToClassifierRSrc)
 */
#include "opmodelgbp/gbp/RuleToClassifierRSrc.hpp"

namespace opmodelgbp {
namespace gbp {

class Rule
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Rule
     */
    static const opflex::modb::class_id_t CLASS_ID = 34;

    /**
     * Check whether name has been set
     * @return true if name has been set
     */
    bool isNameSet()
    {
        return isPropertySet(1114113ul;
    }

    /**
     * Get the value of name if it has been set.
     * @return the value of name or boost::none if not set
     */
    boost::optional<const std::string&> getName()
    {
        if (isNameSet())
            return getProperty(1114113ul);
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
    opmodelgbp::gbp::Rule& setName(const std::string& newValue)
    {
        setProperty(1114113ul, newValue);
        return *this;
    }

    /**
     * Check whether order has been set
     * @return true if order has been set
     */
    bool isOrderSet()
    {
        return isPropertySet(1114114ul;
    }

    /**
     * Get the value of order if it has been set.
     * @return the value of order or boost::none if not set
     */
    boost::optional<uint32_t> getOrder()
    {
        if (isOrderSet())
            return (uint32_t)getProperty(1114114ul);
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
    opmodelgbp::gbp::Rule& setOrder(uint32_t newValue)
    {
        setProperty(1114114ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of Rule.
     * This should not typically be called from user code.
     */
    Rule(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class Rule

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_RULE_HPP
