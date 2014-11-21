/**
 * 
 * SOME COPYRIGHT
 * 
 * RoutingDomain.hpp
 * 
 * generated RoutingDomain.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_ROUTINGDOMAIN_HPP
#define GI_GBP_ROUTINGDOMAIN_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(gbpe/InstContext)
 */
#include "opmodelgbp/gbpe/InstContext.hpp"
/*
 * contains: item:mclass(gbp/BridgeDomainFromNetworkRTgt)
 */
#include "opmodelgbp/gbp/BridgeDomainFromNetworkRTgt.hpp"
/*
 * contains: item:mclass(gbp/EpGroupFromNetworkRTgt)
 */
#include "opmodelgbp/gbp/EpGroupFromNetworkRTgt.hpp"
/*
 * contains: item:mclass(gbp/SubnetsFromNetworkRTgt)
 */
#include "opmodelgbp/gbp/SubnetsFromNetworkRTgt.hpp"

namespace opmodelgbp {
namespace gbp {

class RoutingDomain
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for RoutingDomain
     */
    static const opflex::modb::class_id_t CLASS_ID = 71;

    /**
     * Check whether name has been set
     * @return true if name has been set
     */
    bool isNameSet()
    {
        return isPropertySet(2326529ul;
    }

    /**
     * Get the value of name if it has been set.
     * @return the value of name or boost::none if not set
     */
    boost::optional<const std::string&> getName()
    {
        if (isNameSet())
            return getProperty(2326529ul);
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
    opmodelgbp::gbp::RoutingDomain& setName(const std::string& newValue)
    {
        setProperty(2326529ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of RoutingDomain.
     * This should not typically be called from user code.
     */
    RoutingDomain(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class RoutingDomain

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_ROUTINGDOMAIN_HPP
