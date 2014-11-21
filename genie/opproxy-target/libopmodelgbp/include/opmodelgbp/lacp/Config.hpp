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
#ifndef GI_LACP_CONFIG_HPP
#define GI_LACP_CONFIG_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace lacp {

class Config
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Config
     */
    static const opflex::modb::class_id_t CLASS_ID = 17;

    /**
     * Check whether controlBits has been set
     * @return true if controlBits has been set
     */
    bool isControlBitsSet()
    {
        return isPropertySet(557060ul;
    }

    /**
     * Get the value of controlBits if it has been set.
     * @return the value of controlBits or boost::none if not set
     */
    boost::optional<const uint8_t> getControlBits()
    {
        if (isControlBitsSet())
            return (const uint8_t)getProperty(557060ul);
        return boost::none;
    }

    /**
     * Set controlBits to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lacp::Config& setControlBits(const uint8_t newValue)
    {
        setProperty(557060ul, newValue);
        return *this;
    }

    /**
     * Check whether maxLinks has been set
     * @return true if maxLinks has been set
     */
    bool isMaxLinksSet()
    {
        return isPropertySet(557058ul;
    }

    /**
     * Get the value of maxLinks if it has been set.
     * @return the value of maxLinks or boost::none if not set
     */
    boost::optional<uint16_t> getMaxLinks()
    {
        if (isMaxLinksSet())
            return (uint16_t)getProperty(557058ul);
        return boost::none;
    }

    /**
     * Set maxLinks to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lacp::Config& setMaxLinks(uint16_t newValue)
    {
        setProperty(557058ul, newValue);
        return *this;
    }

    /**
     * Check whether minLinks has been set
     * @return true if minLinks has been set
     */
    bool isMinLinksSet()
    {
        return isPropertySet(557057ul;
    }

    /**
     * Get the value of minLinks if it has been set.
     * @return the value of minLinks or boost::none if not set
     */
    boost::optional<uint16_t> getMinLinks()
    {
        if (isMinLinksSet())
            return (uint16_t)getProperty(557057ul);
        return boost::none;
    }

    /**
     * Set minLinks to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lacp::Config& setMinLinks(uint16_t newValue)
    {
        setProperty(557057ul, newValue);
        return *this;
    }

    /**
     * Check whether mode has been set
     * @return true if mode has been set
     */
    bool isModeSet()
    {
        return isPropertySet(557059ul;
    }

    /**
     * Get the value of mode if it has been set.
     * @return the value of mode or boost::none if not set
     */
    boost::optional<const uint8_t> getMode()
    {
        if (isModeSet())
            return (const uint8_t)getProperty(557059ul);
        return boost::none;
    }

    /**
     * Set mode to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lacp::Config& setMode(const uint8_t newValue)
    {
        setProperty(557059ul, newValue);
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

} // namespace lacp
} // namespace opmodelgbp
#endif // GI_LACP_CONFIG_HPP
