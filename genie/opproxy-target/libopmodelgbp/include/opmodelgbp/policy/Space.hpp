/**
 * 
 * SOME COPYRIGHT
 * 
 * Space.hpp
 * 
 * generated Space.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_POLICY_SPACE_HPP
#define GI_POLICY_SPACE_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(gbpe/L24Classifier)
 */
#include "opmodelgbp/gbpe/L24Classifier.hpp"
/*
 * contains: item:mclass(gbp/Classifier)
 */
#include "opmodelgbp/gbp/Classifier.hpp"
/*
 * contains: item:mclass(gbp/Contract)
 */
#include "opmodelgbp/gbp/Contract.hpp"
/*
 * contains: item:mclass(gbp/EpGroup)
 */
#include "opmodelgbp/gbp/EpGroup.hpp"
/*
 * contains: item:mclass(gbp/BridgeDomain)
 */
#include "opmodelgbp/gbp/BridgeDomain.hpp"
/*
 * contains: item:mclass(gbp/FloodDomain)
 */
#include "opmodelgbp/gbp/FloodDomain.hpp"
/*
 * contains: item:mclass(gbp/RoutingDomain)
 */
#include "opmodelgbp/gbp/RoutingDomain.hpp"
/*
 * contains: item:mclass(gbp/Subnets)
 */
#include "opmodelgbp/gbp/Subnets.hpp"

namespace opmodelgbp {
namespace policy {

class Space
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Space
     */
    static const opflex::modb::class_id_t CLASS_ID = 80;

    /**
     * Check whether name has been set
     * @return true if name has been set
     */
    bool isNameSet()
    {
        return isPropertySet(2621441ul;
    }

    /**
     * Get the value of name if it has been set.
     * @return the value of name or boost::none if not set
     */
    boost::optional<const std::string&> getName()
    {
        if (isNameSet())
            return getProperty(2621441ul);
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
    opmodelgbp::policy::Space& setName(const std::string& newValue)
    {
        setProperty(2621441ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of Space.
     * This should not typically be called from user code.
     */
    Space(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class Space

} // namespace policy
} // namespace opmodelgbp
#endif // GI_POLICY_SPACE_HPP
