/**
 * 
 * SOME COPYRIGHT
 * 
 * RuleToClassifierRRes.hpp
 * 
 * generated RuleToClassifierRRes.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_RULETOCLASSIFIERRRES_HPP
#define GI_GBP_RULETOCLASSIFIERRRES_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace gbp {

class RuleToClassifierRRes
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for RuleToClassifierRRes
     */
    static const opflex::modb::class_id_t CLASS_ID = 47;

    /**
     * Check whether role has been set
     * @return true if role has been set
     */
    bool isRoleSet()
    {
        return isPropertySet(1540098ul;
    }

    /**
     * Get the value of role if it has been set.
     * @return the value of role or boost::none if not set
     */
    boost::optional<const uint8_t> getRole()
    {
        if (isRoleSet())
            return (const uint8_t)getProperty(1540098ul);
        return boost::none;
    }

    /**
     * Set role to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::RuleToClassifierRRes& setRole(const uint8_t newValue)
    {
        setProperty(1540098ul, newValue);
        return *this;
    }

    /**
     * Check whether type has been set
     * @return true if type has been set
     */
    bool isTypeSet()
    {
        return isPropertySet(1540097ul;
    }

    /**
     * Get the value of type if it has been set.
     * @return the value of type or boost::none if not set
     */
    boost::optional<const uint8_t> getType()
    {
        if (isTypeSet())
            return (const uint8_t)getProperty(1540097ul);
        return boost::none;
    }

    /**
     * Set type to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::RuleToClassifierRRes& setType(const uint8_t newValue)
    {
        setProperty(1540097ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of RuleToClassifierRRes.
     * This should not typically be called from user code.
     */
    RuleToClassifierRRes(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class RuleToClassifierRRes

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_RULETOCLASSIFIERRRES_HPP
