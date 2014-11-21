/**
 * 
 * SOME COPYRIGHT
 * 
 * Config.hpp
 * 
 * generated Config.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_STP_CONFIG_HPP
#define GI_STP_CONFIG_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace stp {

class Config
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Config
     */
    static const opflex::modb::class_id_t CLASS_ID = 16;

    /**
     * Check whether bpduFilter has been set
     * @return true if bpduFilter has been set
     */
    bool isBpduFilterSet()
    {
        return isPropertySet(524290ul;
    }

    /**
     * Get the value of bpduFilter if it has been set.
     * @return the value of bpduFilter or boost::none if not set
     */
    boost::optional<const uint8_t> getBpduFilter()
    {
        if (isBpduFilterSet())
            return (const uint8_t)getProperty(524290ul);
        return boost::none;
    }

    /**
     * Set bpduFilter to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::stp::Config& setBpduFilter(const uint8_t newValue)
    {
        setProperty(524290ul, newValue);
        return *this;
    }

    /**
     * Check whether bpduGuard has been set
     * @return true if bpduGuard has been set
     */
    bool isBpduGuardSet()
    {
        return isPropertySet(524289ul;
    }

    /**
     * Get the value of bpduGuard if it has been set.
     * @return the value of bpduGuard or boost::none if not set
     */
    boost::optional<const uint8_t> getBpduGuard()
    {
        if (isBpduGuardSet())
            return (const uint8_t)getProperty(524289ul);
        return boost::none;
    }

    /**
     * Set bpduGuard to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::stp::Config& setBpduGuard(const uint8_t newValue)
    {
        setProperty(524289ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of Config.
     * This should not typically be called from user code.
     */
    Config(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class Config

} // namespace stp
} // namespace opmodelgbp
#endif // GI_STP_CONFIG_HPP
