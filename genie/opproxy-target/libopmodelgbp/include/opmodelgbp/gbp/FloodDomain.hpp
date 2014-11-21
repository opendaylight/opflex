/**
 * 
 * SOME COPYRIGHT
 * 
 * FloodDomain.hpp
 * 
 * generated FloodDomain.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_FLOODDOMAIN_HPP
#define GI_GBP_FLOODDOMAIN_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(gbp/EpGroupFromNetworkRTgt)
 */
#include "opmodelgbp/gbp/EpGroupFromNetworkRTgt.hpp"
/*
 * contains: item:mclass(gbp/FloodDomainToNetworkRSrc)
 */
#include "opmodelgbp/gbp/FloodDomainToNetworkRSrc.hpp"
/*
 * contains: item:mclass(gbp/SubnetsFromNetworkRTgt)
 */
#include "opmodelgbp/gbp/SubnetsFromNetworkRTgt.hpp"

namespace opmodelgbp {
namespace gbp {

class FloodDomain
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for FloodDomain
     */
    static const opflex::modb::class_id_t CLASS_ID = 50;

    /**
     * Check whether name has been set
     * @return true if name has been set
     */
    bool isNameSet()
    {
        return isPropertySet(1638401ul;
    }

    /**
     * Get the value of name if it has been set.
     * @return the value of name or boost::none if not set
     */
    boost::optional<const std::string&> getName()
    {
        if (isNameSet())
            return getProperty(1638401ul);
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
    opmodelgbp::gbp::FloodDomain& setName(const std::string& newValue)
    {
        setProperty(1638401ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of FloodDomain.
     * This should not typically be called from user code.
     */
    FloodDomain(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class FloodDomain

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_FLOODDOMAIN_HPP
