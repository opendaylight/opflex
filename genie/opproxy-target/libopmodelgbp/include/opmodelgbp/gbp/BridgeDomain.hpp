/**
 * 
 * SOME COPYRIGHT
 * 
 * BridgeDomain.hpp
 * 
 * generated BridgeDomain.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_BRIDGEDOMAIN_HPP
#define GI_GBP_BRIDGEDOMAIN_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(gbpe/InstContext)
 */
#include "opmodelgbp/gbpe/InstContext.hpp"
/*
 * contains: item:mclass(gbp/BridgeDomainToNetworkRSrc)
 */
#include "opmodelgbp/gbp/BridgeDomainToNetworkRSrc.hpp"
/*
 * contains: item:mclass(gbp/EpGroupFromNetworkRTgt)
 */
#include "opmodelgbp/gbp/EpGroupFromNetworkRTgt.hpp"
/*
 * contains: item:mclass(gbp/FloodDomainFromNetworkRTgt)
 */
#include "opmodelgbp/gbp/FloodDomainFromNetworkRTgt.hpp"
/*
 * contains: item:mclass(gbp/SubnetsFromNetworkRTgt)
 */
#include "opmodelgbp/gbp/SubnetsFromNetworkRTgt.hpp"

namespace opmodelgbp {
namespace gbp {

class BridgeDomain
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for BridgeDomain
     */
    static const opflex::modb::class_id_t CLASS_ID = 36;

    /**
     * Check whether name has been set
     * @return true if name has been set
     */
    bool isNameSet()
    {
        return isPropertySet(1179649ul;
    }

    /**
     * Get the value of name if it has been set.
     * @return the value of name or boost::none if not set
     */
    boost::optional<const std::string&> getName()
    {
        if (isNameSet())
            return getProperty(1179649ul);
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
    opmodelgbp::gbp::BridgeDomain& setName(const std::string& newValue)
    {
        setProperty(1179649ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of BridgeDomain.
     * This should not typically be called from user code.
     */
    BridgeDomain(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class BridgeDomain

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_BRIDGEDOMAIN_HPP
