/**
 * 
 * SOME COPYRIGHT
 * 
 * LocalL3Ep.hpp
 * 
 * generated LocalL3Ep.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_EPDR_LOCALL3EP_HPP
#define GI_EPDR_LOCALL3EP_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(epdr/EndPointToGroupRSrc)
 */
#include "opmodelgbp/epdr/EndPointToGroupRSrc.hpp"

namespace opmodelgbp {
namespace epdr {

class LocalL3Ep
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for LocalL3Ep
     */
    static const opflex::modb::class_id_t CLASS_ID = 60;

    /**
     * Check whether ip has been set
     * @return true if ip has been set
     */
    bool isIpSet()
    {
        return isPropertySet(1966083ul;
    }

    /**
     * Get the value of ip if it has been set.
     * @return the value of ip or boost::none if not set
     */
    boost::optional<const std::string&> getIp()
    {
        if (isIpSet())
            return getProperty(1966083ul);
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
    opmodelgbp::epdr::LocalL3Ep& setIp(const std::string& newValue)
    {
        setProperty(1966083ul, newValue);
        return *this;
    }

    /**
     * Check whether mac has been set
     * @return true if mac has been set
     */
    bool isMacSet()
    {
        return isPropertySet(1966082ul;
    }

    /**
     * Get the value of mac if it has been set.
     * @return the value of mac or boost::none if not set
     */
    boost::optional<const opflex::modb::MAC&> getMac()
    {
        if (isMacSet())
            return getProperty(1966082ul);
        return boost::none;
    }

    /**
     * Set mac to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epdr::LocalL3Ep& setMac(const opflex::modb::MAC& newValue)
    {
        setProperty(1966082ul, newValue);
        return *this;
    }

    /**
     * Check whether uuid has been set
     * @return true if uuid has been set
     */
    bool isUuidSet()
    {
        return isPropertySet(1966081ul;
    }

    /**
     * Get the value of uuid if it has been set.
     * @return the value of uuid or boost::none if not set
     */
    boost::optional<const std::string&> getUuid()
    {
        if (isUuidSet())
            return getProperty(1966081ul);
        return boost::none;
    }

    /**
     * Set uuid to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epdr::LocalL3Ep& setUuid(const std::string& newValue)
    {
        setProperty(1966081ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of LocalL3Ep.
     * This should not typically be called from user code.
     */
    LocalL3Ep(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class LocalL3Ep

} // namespace epdr
} // namespace opmodelgbp
#endif // GI_EPDR_LOCALL3EP_HPP
