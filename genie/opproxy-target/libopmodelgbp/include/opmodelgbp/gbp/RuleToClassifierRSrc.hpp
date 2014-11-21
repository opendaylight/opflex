/**
 * 
 * SOME COPYRIGHT
 * 
 * RuleToClassifierRSrc.hpp
 * 
 * generated RuleToClassifierRSrc.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_RULETOCLASSIFIERRSRC_HPP
#define GI_GBP_RULETOCLASSIFIERRSRC_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace gbp {

class RuleToClassifierRSrc
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for RuleToClassifierRSrc
     */
    static const opflex::modb::class_id_t CLASS_ID = 38;

    /**
     * Check whether role has been set
     * @return true if role has been set
     */
    bool isRoleSet()
    {
        return isPropertySet(1245186ul;
    }

    /**
     * Get the value of role if it has been set.
     * @return the value of role or boost::none if not set
     */
    boost::optional<const uint8_t> getRole()
    {
        if (isRoleSet())
            return (const uint8_t)getProperty(1245186ul);
        return boost::none;
    }

    /**
     * Set role to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::RuleToClassifierRSrc& setRole(const uint8_t newValue)
    {
        setProperty(1245186ul, newValue);
        return *this;
    }

    /**
     * Check whether targetClass has been set
     * @return true if targetClass has been set
     */
    bool isTargetClassSet()
    {
        return isPropertySet(1245188ul;
    }

    /**
     * Get the value of targetClass if it has been set.
     * @return the value of targetClass or boost::none if not set
     */
    boost::optional<uint16_t> getTargetClass()
    {
        if (isTargetClassSet())
            return (uint16_t)getProperty(1245188ul);
        return boost::none;
    }

    /**
     * Set targetClass to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::RuleToClassifierRSrc& setTargetClass(uint16_t newValue)
    {
        setProperty(1245188ul, newValue);
        return *this;
    }

    /**
     * Check whether targetName has been set
     * @return true if targetName has been set
     */
    bool isTargetNameSet()
    {
        return isPropertySet(1245187ul;
    }

    /**
     * Get the value of targetName if it has been set.
     * @return the value of targetName or boost::none if not set
     */
    boost::optional<const std::string&> getTargetName()
    {
        if (isTargetNameSet())
            return getProperty(1245187ul);
        return boost::none;
    }

    /**
     * Set targetName to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::RuleToClassifierRSrc& setTargetName(const std::string& newValue)
    {
        setProperty(1245187ul, newValue);
        return *this;
    }

    /**
     * Check whether type has been set
     * @return true if type has been set
     */
    bool isTypeSet()
    {
        return isPropertySet(1245185ul;
    }

    /**
     * Get the value of type if it has been set.
     * @return the value of type or boost::none if not set
     */
    boost::optional<const uint8_t> getType()
    {
        if (isTypeSet())
            return (const uint8_t)getProperty(1245185ul);
        return boost::none;
    }

    /**
     * Set type to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::RuleToClassifierRSrc& setType(const uint8_t newValue)
    {
        setProperty(1245185ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of RuleToClassifierRSrc.
     * This should not typically be called from user code.
     */
    RuleToClassifierRSrc(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class RuleToClassifierRSrc

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_RULETOCLASSIFIERRSRC_HPP
