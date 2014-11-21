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
#ifndef GI_POLICY_UNIVERSE_HPP
#define GI_POLICY_UNIVERSE_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(platform/Config)
 */
#include "opmodelgbp/platform/Config.hpp"
/*
 * contains: item:mclass(policy/Space)
 */
#include "opmodelgbp/policy/Space.hpp"

namespace opmodelgbp {
namespace policy {

class Universe
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Universe
     */
    static const opflex::modb::class_id_t CLASS_ID = 78;

    /**
     * Construct an instance of Universe.
     * This should not typically be called from user code.
     */
    Universe(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class Universe

} // namespace policy
} // namespace opmodelgbp
#endif // GI_POLICY_UNIVERSE_HPP
