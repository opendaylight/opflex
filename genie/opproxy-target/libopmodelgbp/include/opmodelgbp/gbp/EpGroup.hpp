/**
 * 
 * SOME COPYRIGHT
 * 
 * EpGroup.hpp
 * 
 * generated EpGroup.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_EPGROUP_HPP
#define GI_GBP_EPGROUP_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(gbpe/InstContext)
 */
#include "opmodelgbp/gbpe/InstContext.hpp"
/*
 * contains: item:mclass(gbp/EpGroupToNetworkRSrc)
 */
#include "opmodelgbp/gbp/EpGroupToNetworkRSrc.hpp"
/*
 * contains: item:mclass(epdr/EndPointFromGroupRTgt)
 */
#include "opmodelgbp/epdr/EndPointFromGroupRTgt.hpp"
/*
 * contains: item:mclass(gbp/EpGroupToProvContractRSrc)
 */
#include "opmodelgbp/gbp/EpGroupToProvContractRSrc.hpp"
/*
 * contains: item:mclass(gbp/EpGroupToConsContractRSrc)
 */
#include "opmodelgbp/gbp/EpGroupToConsContractRSrc.hpp"

namespace opmodelgbp {
namespace gbp {

class EpGroup
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for EpGroup
     */
    static const opflex::modb::class_id_t CLASS_ID = 32;

    /**
     * Check whether name has been set
     * @return true if name has been set
     */
    bool isNameSet()
    {
        return isPropertySet(1048577ul;
    }

    /**
     * Get the value of name if it has been set.
     * @return the value of name or boost::none if not set
     */
    boost::optional<const std::string&> getName()
    {
        if (isNameSet())
            return getProperty(1048577ul);
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
    opmodelgbp::gbp::EpGroup& setName(const std::string& newValue)
    {
        setProperty(1048577ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of EpGroup.
     * This should not typically be called from user code.
     */
    EpGroup(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class EpGroup

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_EPGROUP_HPP
