/**
 * 
 * SOME COPYRIGHT
 * 
 * InstContext.hpp
 * 
 * generated InstContext.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBPE_INSTCONTEXT_HPP
#define GI_GBPE_INSTCONTEXT_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace gbpe {

class InstContext
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for InstContext
     */
    static const opflex::modb::class_id_t CLASS_ID = 12;

    /**
     * Check whether classid has been set
     * @return true if classid has been set
     */
    bool isClassidSet()
    {
        return isPropertySet(393218ul;
    }

    /**
     * Get the value of classid if it has been set.
     * @return the value of classid or boost::none if not set
     */
    boost::optional<uint32_t> getClassid()
    {
        if (isClassidSet())
            return (uint32_t)getProperty(393218ul);
        return boost::none;
    }

    /**
     * Set classid to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::InstContext& setClassid(uint32_t newValue)
    {
        setProperty(393218ul, newValue);
        return *this;
    }

    /**
     * Check whether vnid has been set
     * @return true if vnid has been set
     */
    bool isVnidSet()
    {
        return isPropertySet(393217ul;
    }

    /**
     * Get the value of vnid if it has been set.
     * @return the value of vnid or boost::none if not set
     */
    boost::optional<uint32_t> getVnid()
    {
        if (isVnidSet())
            return (uint32_t)getProperty(393217ul);
        return boost::none;
    }

    /**
     * Set vnid to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbpe::InstContext& setVnid(uint32_t newValue)
    {
        setProperty(393217ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of InstContext.
     * This should not typically be called from user code.
     */
    InstContext(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class InstContext

} // namespace gbpe
} // namespace opmodelgbp
#endif // GI_GBPE_INSTCONTEXT_HPP
