/**
 * 
 * SOME COPYRIGHT
 * 
 * L3Ep.hpp
 * 
 * generated L3Ep.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_EPR_L3EP_HPP
#define GI_EPR_L3EP_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace epr {

class L3Ep
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for L3Ep
     */
    static const opflex::modb::class_id_t CLASS_ID = 29;

    /**
     * Check whether context has been set
     * @return true if context has been set
     */
    bool isContextSet()
    {
        return isPropertySet(950277ul;
    }

    /**
     * Get the value of context if it has been set.
     * @return the value of context or boost::none if not set
     */
    boost::optional<const std::string&> getContext()
    {
        if (isContextSet())
            return getProperty(950277ul);
        return boost::none;
    }

    /**
     * Set context to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epr::L3Ep& setContext(const std::string& newValue)
    {
        setProperty(950277ul, newValue);
        return *this;
    }

    /**
     * Check whether group has been set
     * @return true if group has been set
     */
    bool isGroupSet()
    {
        return isPropertySet(950275ul;
    }

    /**
     * Get the value of group if it has been set.
     * @return the value of group or boost::none if not set
     */
    boost::optional<const std::string&> getGroup()
    {
        if (isGroupSet())
            return getProperty(950275ul);
        return boost::none;
    }

    /**
     * Set group to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epr::L3Ep& setGroup(const std::string& newValue)
    {
        setProperty(950275ul, newValue);
        return *this;
    }

    /**
     * Check whether ip has been set
     * @return true if ip has been set
     */
    bool isIpSet()
    {
        return isPropertySet(950276ul;
    }

    /**
     * Get the value of ip if it has been set.
     * @return the value of ip or boost::none if not set
     */
    boost::optional<const std::string&> getIp()
    {
        if (isIpSet())
            return getProperty(950276ul);
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
    opmodelgbp::epr::L3Ep& setIp(const std::string& newValue)
    {
        setProperty(950276ul, newValue);
        return *this;
    }

    /**
     * Check whether mac has been set
     * @return true if mac has been set
     */
    bool isMacSet()
    {
        return isPropertySet(950274ul;
    }

    /**
     * Get the value of mac if it has been set.
     * @return the value of mac or boost::none if not set
     */
    boost::optional<const opflex::modb::MAC&> getMac()
    {
        if (isMacSet())
            return getProperty(950274ul);
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
    opmodelgbp::epr::L3Ep& setMac(const opflex::modb::MAC& newValue)
    {
        setProperty(950274ul, newValue);
        return *this;
    }

    /**
     * Check whether uuid has been set
     * @return true if uuid has been set
     */
    bool isUuidSet()
    {
        return isPropertySet(950273ul;
    }

    /**
     * Get the value of uuid if it has been set.
     * @return the value of uuid or boost::none if not set
     */
    boost::optional<const std::string&> getUuid()
    {
        if (isUuidSet())
            return getProperty(950273ul);
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
    opmodelgbp::epr::L3Ep& setUuid(const std::string& newValue)
    {
        setProperty(950273ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of L3Ep.
     * This should not typically be called from user code.
     */
    L3Ep(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class L3Ep

} // namespace epr
} // namespace opmodelgbp
#endif // GI_EPR_L3EP_HPP
