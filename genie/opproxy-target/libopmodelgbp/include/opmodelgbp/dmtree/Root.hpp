/**
 * 
 * SOME COPYRIGHT
 * 
 * Root.hpp
 * 
 * generated Root.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_DMTREE_ROOT_HPP
#define GI_DMTREE_ROOT_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(relator/Universe)
 */
#include "opmodelgbp/relator/Universe.hpp"
/*
 * contains: item:mclass(epdr/L2Discovered)
 */
#include "opmodelgbp/epdr/L2Discovered.hpp"
/*
 * contains: item:mclass(epdr/L3Discovered)
 */
#include "opmodelgbp/epdr/L3Discovered.hpp"
/*
 * contains: item:mclass(epr/L2Universe)
 */
#include "opmodelgbp/epr/L2Universe.hpp"
/*
 * contains: item:mclass(epr/L3Universe)
 */
#include "opmodelgbp/epr/L3Universe.hpp"
/*
 * contains: item:mclass(policy/Universe)
 */
#include "opmodelgbp/policy/Universe.hpp"

namespace opmodelgbp {
namespace dmtree {

class Root
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Root
     */
    static const opflex::modb::class_id_t CLASS_ID = 1;

    /**
     * Construct an instance of Root.
     * This should not typically be called from user code.
     */
    Root(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class Root

} // namespace dmtree
} // namespace opmodelgbp
#endif // GI_DMTREE_ROOT_HPP
