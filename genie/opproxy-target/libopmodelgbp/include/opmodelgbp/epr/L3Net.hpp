/**
 * 
 * SOME COPYRIGHT
 * 
 * L3Net.hpp
 * 
 * generated L3Net.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_EPR_L3NET_HPP
#define GI_EPR_L3NET_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace epr {

class L3Net
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for L3Net
     */
    static const opflex::modb::class_id_t CLASS_ID = 28;

    /**
     * Check whether ip has been set
     * @return true if ip has been set
     */
    bool isIpSet()
    {
        return isPropertySet(917505ul;
    }

    /**
     * Get the value of ip if it has been set.
     * @return the value of ip or boost::none if not set
     */
    boost::optional<const std::string&> getIp()
    {
        if (isIpSet())
            return getProperty(917505ul);
        return boost::none;
    }

    /**
     * Set ip to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epr::L3Net& setIp(const std::string& newValue)
    {
        setProperty(917505ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of L3Net.
     * This should not typically be called from user code.
     */
    L3Net(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class L3Net

} // namespace epr
} // namespace opmodelgbp
#endif // GI_EPR_L3NET_HPP
