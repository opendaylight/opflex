/**
 * 
 * SOME COPYRIGHT
 * 
 * Subject.hpp
 * 
 * generated Subject.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_SUBJECT_HPP
#define GI_GBP_SUBJECT_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(gbp/Rule)
 */
#include "opmodelgbp/gbp/Rule.hpp"

namespace opmodelgbp {
namespace gbp {

class Subject
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Subject
     */
    static const opflex::modb::class_id_t CLASS_ID = 33;

    /**
     * Check whether name has been set
     * @return true if name has been set
     */
    bool isNameSet()
    {
        return isPropertySet(1081345ul;
    }

    /**
     * Get the value of name if it has been set.
     * @return the value of name or boost::none if not set
     */
    boost::optional<const std::string&> getName()
    {
        if (isNameSet())
            return getProperty(1081345ul);
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
    opmodelgbp::gbp::Subject& setName(const std::string& newValue)
    {
        setProperty(1081345ul, newValue);
        return *this;
    }

    /**
     * Construct an instance of Subject.
     * This should not typically be called from user code.
     */
    Subject(
        const opflex::modb::URI& uri)
        : MO(CLASS_ID, uri) { }
}; // class Subject

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_SUBJECT_HPP
