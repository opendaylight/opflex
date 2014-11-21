/**
 * 
 * SOME COPYRIGHT
 * 
 * Contract.hpp
 * 
 * generated Contract.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_CONTRACT_HPP
#define GI_GBP_CONTRACT_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(gbp/Subject)
 */
#include "opmodelgbp/gbp/Subject.hpp"
/*
 * contains: item:mclass(gbp/EpGroupFromProvContractRTgt)
 */
#include "opmodelgbp/gbp/EpGroupFromProvContractRTgt.hpp"
/*
 * contains: item:mclass(gbp/EpGroupFromConsContractRTgt)
 */
#include "opmodelgbp/gbp/EpGroupFromConsContractRTgt.hpp"

namespace opmodelgbp {
namespace gbp {

class Contract
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Contract
     */
    static const opflex::modb::class_id_t CLASS_ID = 31;

    /**
     * Check whether name has been set
     * @return true if name has been set
     */
    bool isNameSet()
    {
        return isPropertySet(1015809ul;
    }

    /**
     * Get the value of name if it has been set.
     * @return the value of name or boost::none if not set
     */
    boost::optional<const std::string&> getName()
    {
        if (isNameSet())
            return getProperty(1015809ul);
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
    opmodelgbp::gbp::Contract& setName(const std::string& newValue)
    {
        setProperty(1015809ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of Contract.
     * This should not typically be called from user code.
     */
    Contract(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class Contract

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_CONTRACT_HPP
