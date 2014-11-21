/**
 * 
 * SOME COPYRIGHT
 * 
 * L3Universe.hpp
 * 
 * generated L3Universe.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_EPR_L3UNIVERSE_HPP
#define GI_EPR_L3UNIVERSE_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(epr/L3Ep)
 */
#include "opmodelgbp/epr/L3Ep.hpp"

namespace opmodelgbp {
namespace epr {

class L3Universe
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for L3Universe
     */
    static const opflex::modb::class_id_t CLASS_ID = 27;

    /**
     * Construct an instance of L3Universe.
     * This should not typically be called from user code.
     */
    L3Universe(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class L3Universe

} // namespace epr
} // namespace opmodelgbp
#endif // GI_EPR_L3UNIVERSE_HPP
