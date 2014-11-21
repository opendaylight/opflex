/**
 * 
 * SOME COPYRIGHT
 * 
 * L3Discovered.hpp
 * 
 * generated L3Discovered.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_EPDR_L3DISCOVERED_HPP
#define GI_EPDR_L3DISCOVERED_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(epdr/LocalL3Ep)
 */
#include "opmodelgbp/epdr/LocalL3Ep.hpp"

namespace opmodelgbp {
namespace epdr {

class L3Discovered
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for L3Discovered
     */
    static const opflex::modb::class_id_t CLASS_ID = 20;

    /**
     * Construct an instance of L3Discovered.
     * This should not typically be called from user code.
     */
    L3Discovered(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class L3Discovered

} // namespace epdr
} // namespace opmodelgbp
#endif // GI_EPDR_L3DISCOVERED_HPP
