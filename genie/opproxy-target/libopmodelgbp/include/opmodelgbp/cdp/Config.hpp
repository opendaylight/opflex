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
#ifndef GI_CDP_CONFIG_HPP
#define GI_CDP_CONFIG_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace cdp {

class Config
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Config
     */
    static const opflex::modb::class_id_t CLASS_ID = 9;

    /**
     * Check whether state has been set
     * @return true if state has been set
     */
    bool isStateSet()
    {
        return isPropertySet(294913ul;
    }

    /**
     * Get the value of state if it has been set.
     * @return the value of state or boost::none if not set
     */
    boost::optional<const uint8_t> getState()
    {
        if (isStateSet())
            return (const uint8_t)getProperty(294913ul);
        return boost::none;
    }

    /**
     * Set state to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::cdp::Config& setState(const uint8_t newValue)
    {
        setProperty(294913ul, newValue);
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

} // namespace cdp
} // namespace opmodelgbp
#endif // GI_CDP_CONFIG_HPP
