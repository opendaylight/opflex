/**
 * 
 * SOME COPYRIGHT
 * 
 * BridgeDomainFromNetworkRTgt.hpp
 * 
 * generated BridgeDomainFromNetworkRTgt.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_BRIDGEDOMAINFROMNETWORKRTGT_HPP
#define GI_GBP_BRIDGEDOMAINFROMNETWORKRTGT_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace gbp {

class BridgeDomainFromNetworkRTgt
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for BridgeDomainFromNetworkRTgt
     */
    static const opflex::modb::class_id_t CLASS_ID = 43;

    /**
     * Check whether role has been set
     * @return true if role has been set
     */
    bool isRoleSet()
    {
        return isPropertySet(1409026ul;
    }

    /**
     * Get the value of role if it has been set.
     * @return the value of role or boost::none if not set
     */
    boost::optional<const uint8_t> getRole()
    {
        if (isRoleSet())
            return (const uint8_t)getProperty(1409026ul);
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
    opmodelgbp::gbp::BridgeDomainFromNetworkRTgt& setRole(const uint8_t newValue)
    {
        setProperty(1409026ul, newValue);
        return *this;
    }

    /**
     * Check whether source has been set
     * @return true if source has been set
     */
    bool isSourceSet()
    {
        return isPropertySet(1409027ul;
    }

    /**
     * Get the value of source if it has been set.
     * @return the value of source or boost::none if not set
     */
    boost::optional<const std::string&> getSource()
    {
        if (isSourceSet())
            return getProperty(1409027ul);
        return boost::none;
    }

    /**
     * Set source to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::BridgeDomainFromNetworkRTgt& setSource(const std::string& newValue)
    {
        setProperty(1409027ul, newValue);
        return *this;
    }

    /**
     * Check whether type has been set
     * @return true if type has been set
     */
    bool isTypeSet()
    {
        return isPropertySet(1409025ul;
    }

    /**
     * Get the value of type if it has been set.
     * @return the value of type or boost::none if not set
     */
    boost::optional<const uint8_t> getType()
    {
        if (isTypeSet())
            return (const uint8_t)getProperty(1409025ul);
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
    opmodelgbp::gbp::BridgeDomainFromNetworkRTgt& setType(const uint8_t newValue)
    {
        setProperty(1409025ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of BridgeDomainFromNetworkRTgt.
     * This should not typically be called from user code.
     */
    BridgeDomainFromNetworkRTgt(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class BridgeDomainFromNetworkRTgt

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_BRIDGEDOMAINFROMNETWORKRTGT_HPP
