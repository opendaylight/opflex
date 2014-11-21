/**
 * 
 * SOME COPYRIGHT
 * 
 * BridgeDomainToNetworkRRes.hpp
 * 
 * generated BridgeDomainToNetworkRRes.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_BRIDGEDOMAINTONETWORKRRES_HPP
#define GI_GBP_BRIDGEDOMAINTONETWORKRRES_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace gbp {

class BridgeDomainToNetworkRRes
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for BridgeDomainToNetworkRRes
     */
    static const opflex::modb::class_id_t CLASS_ID = 48;

    /**
     * Check whether role has been set
     * @return true if role has been set
     */
    bool isRoleSet()
    {
        return isPropertySet(1572866ul;
    }

    /**
     * Get the value of role if it has been set.
     * @return the value of role or boost::none if not set
     */
    boost::optional<const uint8_t> getRole()
    {
        if (isRoleSet())
            return (const uint8_t)getProperty(1572866ul);
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
    opmodelgbp::gbp::BridgeDomainToNetworkRRes& setRole(const uint8_t newValue)
    {
        setProperty(1572866ul, newValue);
        return *this;
    }

    /**
     * Check whether type has been set
     * @return true if type has been set
     */
    bool isTypeSet()
    {
        return isPropertySet(1572865ul;
    }

    /**
     * Get the value of type if it has been set.
     * @return the value of type or boost::none if not set
     */
    boost::optional<const uint8_t> getType()
    {
        if (isTypeSet())
            return (const uint8_t)getProperty(1572865ul);
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
    opmodelgbp::gbp::BridgeDomainToNetworkRRes& setType(const uint8_t newValue)
    {
        setProperty(1572865ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of BridgeDomainToNetworkRRes.
     * This should not typically be called from user code.
     */
    BridgeDomainToNetworkRRes(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class BridgeDomainToNetworkRRes

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_BRIDGEDOMAINTONETWORKRRES_HPP
