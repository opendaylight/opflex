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
#ifndef GI_LLDP_CONFIG_HPP
#define GI_LLDP_CONFIG_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace lldp {

class Config
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Config
     */
    static const opflex::modb::class_id_t CLASS_ID = 15;

    /**
     * Check whether rx has been set
     * @return true if rx has been set
     */
    bool isRxSet()
    {
        return isPropertySet(491521ul;
    }

    /**
     * Get the value of rx if it has been set.
     * @return the value of rx or boost::none if not set
     */
    boost::optional<const uint8_t> getRx()
    {
        if (isRxSet())
            return (const uint8_t)getProperty(491521ul);
        return boost::none;
    }

    /**
     * Set rx to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lldp::Config& setRx(const uint8_t newValue)
    {
        setProperty(491521ul, newValue);
        return *this;
    }

    /**
     * Check whether tx has been set
     * @return true if tx has been set
     */
    bool isTxSet()
    {
        return isPropertySet(491522ul;
    }

    /**
     * Get the value of tx if it has been set.
     * @return the value of tx or boost::none if not set
     */
    boost::optional<const uint8_t> getTx()
    {
        if (isTxSet())
            return (const uint8_t)getProperty(491522ul);
        return boost::none;
    }

    /**
     * Set tx to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::lldp::Config& setTx(const uint8_t newValue)
    {
        setProperty(491522ul, newValue);
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

} // namespace lldp
} // namespace opmodelgbp
#endif // GI_LLDP_CONFIG_HPP
