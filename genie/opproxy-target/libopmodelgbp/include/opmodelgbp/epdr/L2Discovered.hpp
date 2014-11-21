/**
 * 
 * SOME COPYRIGHT
 * 
 * L2Discovered.hpp
 * 
 * generated L2Discovered.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_EPDR_L2DISCOVERED_HPP
#define GI_EPDR_L2DISCOVERED_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(epdr/LocalL2Ep)
 */
#include "opmodelgbp/epdr/LocalL2Ep.hpp"

namespace opmodelgbp {
namespace epdr {

class L2Discovered
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for L2Discovered
     */
    static const opflex::modb::class_id_t CLASS_ID = 18;

    /**
     * Construct an instance of L2Discovered.
     * This should not typically be called from user code.
     */
    L2Discovered(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class L2Discovered

} // namespace epdr
} // namespace opmodelgbp
#endif // GI_EPDR_L2DISCOVERED_HPP
