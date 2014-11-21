/**
 * 
 * SOME COPYRIGHT
 * 
 * Universe.hpp
 * 
 * generated Universe.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_RELATOR_UNIVERSE_HPP
#define GI_RELATOR_UNIVERSE_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(epdr/EndPointToGroupRRes)
 */
#include "opmodelgbp/epdr/EndPointToGroupRRes.hpp"
/*
 * contains: item:mclass(gbp/EpGroupToNetworkRRes)
 */
#include "opmodelgbp/gbp/EpGroupToNetworkRRes.hpp"
/*
 * contains: item:mclass(gbp/RuleToClassifierRRes)
 */
#include "opmodelgbp/gbp/RuleToClassifierRRes.hpp"
/*
 * contains: item:mclass(gbp/BridgeDomainToNetworkRRes)
 */
#include "opmodelgbp/gbp/BridgeDomainToNetworkRRes.hpp"
/*
 * contains: item:mclass(gbp/EpGroupToProvContractRRes)
 */
#include "opmodelgbp/gbp/EpGroupToProvContractRRes.hpp"
/*
 * contains: item:mclass(gbp/FloodDomainToNetworkRRes)
 */
#include "opmodelgbp/gbp/FloodDomainToNetworkRRes.hpp"
/*
 * contains: item:mclass(gbp/EpGroupToConsContractRRes)
 */
#include "opmodelgbp/gbp/EpGroupToConsContractRRes.hpp"
/*
 * contains: item:mclass(gbp/SubnetsToNetworkRRes)
 */
#include "opmodelgbp/gbp/SubnetsToNetworkRRes.hpp"

namespace opmodelgbp {
namespace relator {

class Universe
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Universe
     */
    static const opflex::modb::class_id_t CLASS_ID = 7;

    /**
     * Construct an instance of Universe.
     * This should not typically be called from user code.
     */
    Universe(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class Universe

} // namespace relator
} // namespace opmodelgbp
#endif // GI_RELATOR_UNIVERSE_HPP
