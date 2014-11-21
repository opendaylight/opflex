/**
 * 
 * SOME COPYRIGHT
 * 
 * EpGroupToProvContractRRes.hpp
 * 
 * generated EpGroupToProvContractRRes.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_EPGROUPTOPROVCONTRACTRRES_HPP
#define GI_GBP_EPGROUPTOPROVCONTRACTRRES_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace gbp {

class EpGroupToProvContractRRes
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for EpGroupToProvContractRRes
     */
    static const opflex::modb::class_id_t CLASS_ID = 55;

    /**
     * Check whether role has been set
     * @return true if role has been set
     */
    bool isRoleSet()
    {
        return isPropertySet(1802242ul;
    }

    /**
     * Get the value of role if it has been set.
     * @return the value of role or boost::none if not set
     */
    boost::optional<const uint8_t> getRole()
    {
        if (isRoleSet())
            return (const uint8_t)getProperty(1802242ul);
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
    opmodelgbp::gbp::EpGroupToProvContractRRes& setRole(const uint8_t newValue)
    {
        setProperty(1802242ul, newValue);
        return *this;
    }

    /**
     * Check whether type has been set
     * @return true if type has been set
     */
    bool isTypeSet()
    {
        return isPropertySet(1802241ul;
    }

    /**
     * Get the value of type if it has been set.
     * @return the value of type or boost::none if not set
     */
    boost::optional<const uint8_t> getType()
    {
        if (isTypeSet())
            return (const uint8_t)getProperty(1802241ul);
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
    opmodelgbp::gbp::EpGroupToProvContractRRes& setType(const uint8_t newValue)
    {
        setProperty(1802241ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of EpGroupToProvContractRRes.
     * This should not typically be called from user code.
     */
    EpGroupToProvContractRRes(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class EpGroupToProvContractRRes

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_EPGROUPTOPROVCONTRACTRRES_HPP
