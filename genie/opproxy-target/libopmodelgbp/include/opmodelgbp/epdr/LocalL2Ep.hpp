/**
 * 
 * SOME COPYRIGHT
 * 
 * LocalL2Ep.hpp
 * 
 * generated LocalL2Ep.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_EPDR_LOCALL2EP_HPP
#define GI_EPDR_LOCALL2EP_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(epdr/EndPointToGroupRSrc)
 */
#include "opmodelgbp/epdr/EndPointToGroupRSrc.hpp"

namespace opmodelgbp {
namespace epdr {

class LocalL2Ep
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for LocalL2Ep
     */
    static const opflex::modb::class_id_t CLASS_ID = 49;

    /**
     * Check whether mac has been set
     * @return true if mac has been set
     */
    bool isMacSet()
    {
        return isPropertySet(1605634ul;
    }

    /**
     * Get the value of mac if it has been set.
     * @return the value of mac or boost::none if not set
     */
    boost::optional<const opflex::modb::MAC&> getMac()
    {
        if (isMacSet())
            return getProperty(1605634ul);
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
    opmodelgbp::epdr::LocalL2Ep& setMac(const opflex::modb::MAC& newValue)
    {
        setProperty(1605634ul, newValue);
        return *this;
    }

    /**
     * Check whether uuid has been set
     * @return true if uuid has been set
     */
    bool isUuidSet()
    {
        return isPropertySet(1605633ul;
    }

    /**
     * Get the value of uuid if it has been set.
     * @return the value of uuid or boost::none if not set
     */
    boost::optional<const std::string&> getUuid()
    {
        if (isUuidSet())
            return getProperty(1605633ul);
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
    opmodelgbp::epdr::LocalL2Ep& setUuid(const std::string& newValue)
    {
        setProperty(1605633ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of LocalL2Ep.
     * This should not typically be called from user code.
     */
    LocalL2Ep(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class LocalL2Ep

} // namespace epdr
} // namespace opmodelgbp
#endif // GI_EPDR_LOCALL2EP_HPP
