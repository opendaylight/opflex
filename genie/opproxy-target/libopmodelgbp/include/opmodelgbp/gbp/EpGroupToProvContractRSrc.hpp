/**
 * 
 * SOME COPYRIGHT
 * 
 * EpGroupToProvContractRSrc.hpp
 * 
 * generated EpGroupToProvContractRSrc.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_EPGROUPTOPROVCONTRACTRSRC_HPP
#define GI_GBP_EPGROUPTOPROVCONTRACTRSRC_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace gbp {

class EpGroupToProvContractRSrc
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for EpGroupToProvContractRSrc
     */
    static const opflex::modb::class_id_t CLASS_ID = 51;

    /**
     * Check whether role has been set
     * @return true if role has been set
     */
    bool isRoleSet()
    {
        return isPropertySet(1671170ul;
    }

    /**
     * Get the value of role if it has been set.
     * @return the value of role or boost::none if not set
     */
    boost::optional<const uint8_t> getRole()
    {
        if (isRoleSet())
            return (const uint8_t)getProperty(1671170ul);
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
    opmodelgbp::gbp::EpGroupToProvContractRSrc& setRole(const uint8_t newValue)
    {
        setProperty(1671170ul, newValue);
        return *this;
    }

    /**
     * Check whether targetClass has been set
     * @return true if targetClass has been set
     */
    bool isTargetClassSet()
    {
        return isPropertySet(1671172ul;
    }

    /**
     * Get the value of targetClass if it has been set.
     * @return the value of targetClass or boost::none if not set
     */
    boost::optional<uint16_t> getTargetClass()
    {
        if (isTargetClassSet())
            return (uint16_t)getProperty(1671172ul);
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
    opmodelgbp::gbp::EpGroupToProvContractRSrc& setTargetClass(uint16_t newValue)
    {
        setProperty(1671172ul, newValue);
        return *this;
    }

    /**
     * Check whether targetName has been set
     * @return true if targetName has been set
     */
    bool isTargetNameSet()
    {
        return isPropertySet(1671171ul;
    }

    /**
     * Get the value of targetName if it has been set.
     * @return the value of targetName or boost::none if not set
     */
    boost::optional<const std::string&> getTargetName()
    {
        if (isTargetNameSet())
            return getProperty(1671171ul);
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
    opmodelgbp::gbp::EpGroupToProvContractRSrc& setTargetName(const std::string& newValue)
    {
        setProperty(1671171ul, newValue);
        return *this;
    }

    /**
     * Check whether type has been set
     * @return true if type has been set
     */
    bool isTypeSet()
    {
        return isPropertySet(1671169ul;
    }

    /**
     * Get the value of type if it has been set.
     * @return the value of type or boost::none if not set
     */
    boost::optional<const uint8_t> getType()
    {
        if (isTypeSet())
            return (const uint8_t)getProperty(1671169ul);
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
    opmodelgbp::gbp::EpGroupToProvContractRSrc& setType(const uint8_t newValue)
    {
        setProperty(1671169ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of EpGroupToProvContractRSrc.
     * This should not typically be called from user code.
     */
    EpGroupToProvContractRSrc(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class EpGroupToProvContractRSrc

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_EPGROUPTOPROVCONTRACTRSRC_HPP
