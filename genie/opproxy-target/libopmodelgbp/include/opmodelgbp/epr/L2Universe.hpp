/**
 * 
 * SOME COPYRIGHT
 * 
 * L2Universe.hpp
 * 
 * generated L2Universe.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_EPR_L2UNIVERSE_HPP
#define GI_EPR_L2UNIVERSE_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(epr/L2Ep)
 */
#include "opmodelgbp/epr/L2Ep.hpp"

namespace opmodelgbp {
namespace epr {

class L2Universe
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for L2Universe
     */
    static const opflex::modb::class_id_t CLASS_ID = 26;

    /**
     * Construct an instance of L2Universe.
     * This should not typically be called from user code.
     */
    L2Universe(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class L2Universe

} // namespace epr
} // namespace opmodelgbp
#endif // GI_EPR_L2UNIVERSE_HPP
