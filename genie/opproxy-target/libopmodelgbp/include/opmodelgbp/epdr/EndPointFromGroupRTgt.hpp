/**
 * 
 * SOME COPYRIGHT
 * 
 * EndPointFromGroupRTgt.hpp
 * 
 * generated EndPointFromGroupRTgt.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_EPDR_ENDPOINTFROMGROUPRTGT_HPP
#define GI_EPDR_ENDPOINTFROMGROUPRTGT_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace opmodelgbp {
namespace epdr {

class EndPointFromGroupRTgt
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for EndPointFromGroupRTgt
     */
    static const opflex::modb::class_id_t CLASS_ID = 41;

    /**
     * Check whether role has been set
     * @return true if role has been set
     */
    bool isRoleSet()
    {
        return isPropertySet(1343490ul;
    }

    /**
     * Get the value of role if it has been set.
     * @return the value of role or boost::none if not set
     */
    boost::optional<const uint8_t> getRole()
    {
        if (isRoleSet())
            return (const uint8_t)getProperty(1343490ul);
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
    opmodelgbp::epdr::EndPointFromGroupRTgt& setRole(const uint8_t newValue)
    {
        setProperty(1343490ul, newValue);
        return *this;
    }

    /**
     * Check whether source has been set
     * @return true if source has been set
     */
    bool isSourceSet()
    {
        return isPropertySet(1343491ul;
    }

    /**
     * Get the value of source if it has been set.
     * @return the value of source or boost::none if not set
     */
    boost::optional<const std::string&> getSource()
    {
        if (isSourceSet())
            return getProperty(1343491ul);
        return boost::none;
    }

    /**
     * Set source to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::epdr::EndPointFromGroupRTgt& setSource(const std::string& newValue)
    {
        setProperty(1343491ul, newValue);
        return *this;
    }

    /**
     * Check whether type has been set
     * @return true if type has been set
     */
    bool isTypeSet()
    {
        return isPropertySet(1343489ul;
    }

    /**
     * Get the value of type if it has been set.
     * @return the value of type or boost::none if not set
     */
    boost::optional<const uint8_t> getType()
    {
        if (isTypeSet())
            return (const uint8_t)getProperty(1343489ul);
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
    opmodelgbp::epdr::EndPointFromGroupRTgt& setType(const uint8_t newValue)
    {
        setProperty(1343489ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of EndPointFromGroupRTgt.
     * This should not typically be called from user code.
     */
    EndPointFromGroupRTgt(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class EndPointFromGroupRTgt

} // namespace epdr
} // namespace opmodelgbp
#endif // GI_EPDR_ENDPOINTFROMGROUPRTGT_HPP
