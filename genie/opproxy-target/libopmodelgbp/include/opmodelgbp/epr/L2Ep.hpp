/**
 * 
 * SOME COPYRIGHT
 * 
 * L2Ep.hpp
 * 
 * generated L2Ep.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_EPR_L2EP_HPP
#define GI_EPR_L2EP_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(epr/L3Net)
 */
#include "opmodelgbp/epr/L3Net.hpp"

namespace opmodelgbp {
namespace epr {

class L2Ep
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for L2Ep
     */
    static const opflex::modb::class_id_t CLASS_ID = 25;

    /**
     * Check whether context has been set
     * @return true if context has been set
     */
    bool isContextSet()
    {
        return isPropertySet(819204ul;
    }

    /**
     * Get the value of context if it has been set.
     * @return the value of context or boost::none if not set
     */
    boost::optional<const std::string&> getContext()
    {
        if (isContextSet())
            return getProperty(819204ul);
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
    opmodelgbp::epr::L2Ep& setContext(const std::string& newValue)
    {
        setProperty(819204ul, newValue);
        return *this;
    }

    /**
     * Check whether group has been set
     * @return true if group has been set
     */
    bool isGroupSet()
    {
        return isPropertySet(819203ul;
    }

    /**
     * Get the value of group if it has been set.
     * @return the value of group or boost::none if not set
     */
    boost::optional<const std::string&> getGroup()
    {
        if (isGroupSet())
            return getProperty(819203ul);
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
    opmodelgbp::epr::L2Ep& setGroup(const std::string& newValue)
    {
        setProperty(819203ul, newValue);
        return *this;
    }

    /**
     * Check whether mac has been set
     * @return true if mac has been set
     */
    bool isMacSet()
    {
        return isPropertySet(819202ul;
    }

    /**
     * Get the value of mac if it has been set.
     * @return the value of mac or boost::none if not set
     */
    boost::optional<const opflex::modb::MAC&> getMac()
    {
        if (isMacSet())
            return getProperty(819202ul);
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
    opmodelgbp::epr::L2Ep& setMac(const opflex::modb::MAC& newValue)
    {
        setProperty(819202ul, newValue);
        return *this;
    }

    /**
     * Check whether uuid has been set
     * @return true if uuid has been set
     */
    bool isUuidSet()
    {
        return isPropertySet(819201ul;
    }

    /**
     * Get the value of uuid if it has been set.
     * @return the value of uuid or boost::none if not set
     */
    boost::optional<const std::string&> getUuid()
    {
        if (isUuidSet())
            return getProperty(819201ul);
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
    opmodelgbp::epr::L2Ep& setUuid(const std::string& newValue)
    {
        setProperty(819201ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of L2Ep.
     * This should not typically be called from user code.
     */
    L2Ep(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class L2Ep

} // namespace epr
} // namespace opmodelgbp
#endif // GI_EPR_L2EP_HPP
