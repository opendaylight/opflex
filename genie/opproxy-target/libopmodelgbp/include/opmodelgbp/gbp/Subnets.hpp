/**
 * 
 * SOME COPYRIGHT
 * 
 * Subnets.hpp
 * 
 * generated Subnets.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_SUBNETS_HPP
#define GI_GBP_SUBNETS_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(gbp/EpGroupFromNetworkRTgt)
 */
#include "opmodelgbp/gbp/EpGroupFromNetworkRTgt.hpp"
/*
 * contains: item:mclass(gbp/Subnet)
 */
#include "opmodelgbp/gbp/Subnet.hpp"
/*
 * contains: item:mclass(gbp/SubnetsToNetworkRSrc)
 */
#include "opmodelgbp/gbp/SubnetsToNetworkRSrc.hpp"

namespace opmodelgbp {
namespace gbp {

class Subnets
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Subnets
     */
    static const opflex::modb::class_id_t CLASS_ID = 73;

    /**
     * Check whether name has been set
     * @return true if name has been set
     */
    bool isNameSet()
    {
        return isPropertySet(2392065ul;
    }

    /**
     * Get the value of name if it has been set.
     * @return the value of name or boost::none if not set
     */
    boost::optional<const std::string&> getName()
    {
        if (isNameSet())
            return getProperty(2392065ul);
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
    opmodelgbp::gbp::Subnets& setName(const std::string& newValue)
    {
        setProperty(2392065ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of Subnets.
     * This should not typically be called from user code.
     */
    Subnets(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class Subnets

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_SUBNETS_HPP
