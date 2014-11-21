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
#ifndef GI_PLATFORM_CONFIG_HPP
#define GI_PLATFORM_CONFIG_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(cdp/Config)
 */
#include "opmodelgbp/cdp/Config.hpp"
/*
 * contains: item:mclass(l2/Config)
 */
#include "opmodelgbp/l2/Config.hpp"
/*
 * contains: item:mclass(lldp/Config)
 */
#include "opmodelgbp/lldp/Config.hpp"
/*
 * contains: item:mclass(stp/Config)
 */
#include "opmodelgbp/stp/Config.hpp"
/*
 * contains: item:mclass(lacp/Config)
 */
#include "opmodelgbp/lacp/Config.hpp"

namespace opmodelgbp {
namespace platform {

class Config
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Config
     */
    static const opflex::modb::class_id_t CLASS_ID = 10;

    /**
     * Check whether name has been set
     * @return true if name has been set
     */
    bool isNameSet()
    {
        return isPropertySet(327681ul;
    }

    /**
     * Get the value of name if it has been set.
     * @return the value of name or boost::none if not set
     */
    boost::optional<const std::string&> getName()
    {
        if (isNameSet())
            return getProperty(327681ul);
        return boost::none;
    }

    /**
     * Set name to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::platform::Config& setName(const std::string& newValue)
    {
        setProperty(327681ul, newValue);
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

} // namespace platform
} // namespace opmodelgbp
#endif // GI_PLATFORM_CONFIG_HPP
