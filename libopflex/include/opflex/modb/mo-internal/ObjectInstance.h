/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file ObjectInstance.h
 * @brief Interface definition file for ObjectInstance
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef MODB_OBJECTINSTANCE_H_
#define MODB_OBJECTINSTANCE_H_

#include <string>
#include <utility>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/cstdint.hpp>
#include <boost/variant.hpp>

#include "opflex/modb/PropertyInfo.h"
#include "opflex/modb/URI.h"
#include "opflex/modb/MAC.h"
#include "opflex/ofcore/OFTypes.h"

namespace opflex {
namespace modb {

/**
 * A tuple containing the key for a property value
 */
typedef boost::tuple<PropertyInfo::property_type_t,
                     PropertyInfo::cardinality_t,
                     prop_id_t> prop_key_t;

/**
 * A URI reference containing a class ID and a URI pair
 */
typedef std::pair<class_id_t, URI> reference_t;

/**
 * Compute a hash value for the prop key, making prop_key_t suitable
 * as a key in an unordered_map
 */
size_t hash_value(prop_key_t const& key);

/**
 * Compute a hash value for the reference_t, making it suitable as a
 * key in a boost::unordered_map
 */
size_t hash_value(reference_t const& key);

} /* namespace modb */
} /* namespace opflex */

#if __cplusplus > 199711L

namespace std {
/**
 * Template specialization for std::hash<opflex::modb::prop_key_t>, making
 * prop_key_t suitable as a key in a std::unordered_map
 */
template<> struct hash<opflex::modb::prop_key_t> {
    /**
     * Hash the opflex::modb::prop_key_t
     */
    std::size_t operator()(const opflex::modb::prop_key_t& u) const {
        return opflex::modb::hash_value(u);
    }
};

/**
 * Template specialization for std::hash<opflex::modb::reference_t>, making
 * it suitable as a key in a std::unordered_map
 */
template<> struct hash<opflex::modb::reference_t> {
    /**
     * Hash the opflex::modb::reference_t
     */
    std::size_t operator()(const opflex::modb::reference_t& p) const {
        return opflex::modb::hash_value(p);
    }
};

} /* namespace std */

#endif

namespace opflex {
namespace modb {

namespace mointernal {

/**
 * @brief An internal instance of an object in the managed object store.
 *
 * While inside the object store, an object instance should never be
 * modified; only const references should be generated.  To modify, we
 * atomically update the pointer to point to a modified copy.
 */
class ObjectInstance {
public:
    /**
     * Construct an empty object represented the specified class
     *
     * @param class_id_ the class ID for the object
     */
    ObjectInstance(class_id_t class_id_)
        : class_id(class_id_), local(true) { }

    /**
     * Construct an empty object represented the specified class and
     * using the provided local flag
     *
     * @param class_id_ the class ID for the object
     * @param local_ True if the instance is locally-created
     */
    ObjectInstance(class_id_t class_id_, bool local_)
        : class_id(class_id_), local(local_) { }

    /**
     * Get the class ID for this object instance
     *
     * @return the class ID
     */
    class_id_t getClassId() const { return class_id; }

    /**
     * Return whether the object was from the remote policy repository
     * or was written locally.
     *
     * @return true if the object is local
     */
    bool isLocal() const { return local; }

    /**
     * Check whether the given property is set.  If the property is
     * vector-valued, this will return false if the vector is zero
     * length.
     *
     * @param prop_id the property ID to check
     * @param cardinality the cardinality of the property to check
     * @param type the type of the property
     */
    bool isSet(prop_id_t prop_id,
               PropertyInfo::property_type_t type,
               PropertyInfo::cardinality_t cardinality = PropertyInfo::SCALAR) const;

    /**
     * Unset the given property.  If its a vector, the vector is
     * emptied.  If its a scalar, the scalar is unset.
     *
     * @param prop_id the property ID to reset
     * @param type the type of the property
     * @param cardinality the cardinality of the property to unset
     * @return true if the property was alread set before
     */
    bool unset(prop_id_t prop_id,
               PropertyInfo::property_type_t type,
               PropertyInfo::cardinality_t cardinality);

    /**
     * Get the unsigned 64-bit valued property for prop_name.
     *
     * @param prop_id the property ID to look up
     * @return the property value
     * @throws std::out_of_range if no such element is present.
     */
    uint64_t getUInt64(prop_id_t prop_id) const;

    /**
     * For a vector-valued 64-bit unsigned property, get the specified
     * property value at the specified index
     *
     * @param prop_id the property ID to look up
     * @param index the vector index to retrieve
     * @return the property value
     * @throws std::out_of_range if no such element is present.
     */
    uint64_t getUInt64(prop_id_t prop_id, size_t index) const;

    /**
     * Get the number of unsigned 64-bit values for the specified
     * property
     *
     * @param prop_id the property ID to look up
     * @return the number of elements
     */
    size_t getUInt64Size(prop_id_t prop_id) const;

    /**
     * Get the signed 64-bit valued property for prop_name.
     *
     * @param prop_id the property ID to look up
     * @return the property value
     * @throws std::out_of_range if no such element is present.
     */
    int64_t getInt64(prop_id_t prop_id) const;

    /**
     * For a vector-valued 64-bit signed property, get the specified
     * property value at the specified index
     *
     * @param prop_id the property ID to look up
     * @param index the vector index to retrieve
     * @return the property value
     * @throws std::out_of_range if no such element is present.
     */
    int64_t getInt64(prop_id_t prop_id, size_t index) const;

    /**
     * Get the number of signed 64-bit values for the specified
     * property
     *
     * @param prop_id the property ID to look up
     * @return the number of elements
     */
    size_t getInt64Size(prop_id_t prop_id) const;

    /**
     * Get the string-valued property for prop_name.
     * @param prop_id the property ID to look up
     * @return the property value
     * @throws std::out_of_range if no such element is present.
     */
    const std::string& getString(prop_id_t prop_id) const;

    /**
     * For a vector-valued string property, get the specified property
     * value at the specified index
     *
     * @param prop_id the property ID to look up
     * @param index the vector index to retrieve
     * @return the property value
     * @throws std::out_of_range if no such element is present.
     */
    const std::string& getString(prop_id_t prop_id, size_t index) const;

    /**
     * Get the number of string values for the specified property
     *
     * @param prop_id the property ID to look up
     * @return the number of elements
     */
    size_t getStringSize(prop_id_t prop_id) const;

    /**
     * Get the reference-valued property for prop_name.
     * @param prop_id the property ID to look up
     * @return the property value
     * @throws std::out_of_range if no such element is present.
     */
    reference_t getReference(prop_id_t prop_id) const;

    /**
     * For a vector-valued reference property, get the specified property
     * value at the specified index
     *
     * @param prop_id the property ID to look up
     * @param index the vector index to retrieve
     * @return the property value
     * @throws std::out_of_range if no such element is present.
     */
    reference_t getReference(prop_id_t prop_id, size_t index) const;

    /**
     * Get the number of reference values for the specified property
     *
     * @param prop_id the property ID to look up
     * @return the number of elements
     */
    size_t getReferenceSize(prop_id_t prop_id) const;

    /**
     * Get the MAC-address-valued property for prop_name.
     *
     * @param prop_id the property ID to look up
     * @return the property value
     * @throws std::out_of_range if no such element is present.
     */
    const MAC& getMAC(prop_id_t prop_id) const;

    /**
     * For a vector-valued MAC address property, get the specified
     * property value at the specified index
     *
     * @param prop_id the property ID to look up
     * @param index the vector index to retrieve
     * @return the property value
     * @throws std::out_of_range if no such element is present.
     */
    const MAC& getMAC(prop_id_t prop_id, size_t index) const;

    /**
     * Get the number of MAC address values for the specified
     * property
     *
     * @param prop_id the property ID to look up
     * @return the number of elements
     */
    size_t getMACSize(prop_id_t prop_id) const;

    /**
     * Set the uint64-valued parameter to the specified value
     *
     * @param prop_id the property ID to set
     * @param value the value to set
     * @return a reference to this object that can be used to chain
     * the calls
     */
    void setUInt64(prop_id_t prop_id, uint64_t value);

    /**
     * Set the uint64-vector-valued parameter to the specified value
     *
     * @param prop_id the property ID to set
     * @param value the value to set
     * @return a reference to this object that can be used to chain
     * the calls
     */
    void setUInt64(prop_id_t prop_id, const std::vector<uint64_t>& value);

    /**
     * Set the string-valued parameter to the specified value
     *
     * @param prop_id the property ID to set
     * @param value the value to set
     * @return a reference to this object that can be used to chain
     * the calls
     */
    void setString(prop_id_t prop_id, const std::string& value);

    /**
     * Set the string-vector-valued parameter to the specified vector
     *
     * @param prop_id the property ID to set
     * @param value the value to set
     * @return a reference to this object that can be used to chain
     * the calls
     */
    void setString(prop_id_t prop_id, const std::vector<std::string>& value);

    /**
     * Set the int64-valued parameter to the specified value
     *
     * @param prop_id the property ID to set
     * @param value the value to set
     * @return a reference to this object that can be used to chain
     * the calls
     */
    void setInt64(prop_id_t prop_id, int64_t value);

    /**
     * Set the int64-vector-valued parameter to the specified value
     *
     * @param prop_id the property ID to set
     * @param value the value to set
     * @return a reference to this object that can be used to chain
     * the calls
     */
    void setInt64(prop_id_t prop_id, const std::vector<int64_t>& value);

    /**
     * Set the reference-valued parameter to the specified value
     *
     * @param prop_id the property ID to set
     * @param class_id the class_id of the reference
     * @param uri the URI of the reference
     * @return a reference to this object that can be used to chain
     * the calls
     */
    void setReference(prop_id_t prop_id,
                      class_id_t class_id, const URI& uri);

    /**
     * Set the reference-vector-valued parameter to the specified
     * vector
     *
     * @param prop_id the property ID to set
     * @param value the value to set
     * @return a reference to this object that can be used to chain
     * the calls
     */
    void setReference(prop_id_t prop_id,
                      const std::vector<reference_t>& value);

    /**
     * Set the MAC address-valued parameter to the specified value
     *
     * @param prop_id the property ID to set
     * @param value the value to set
     * @return a reference to this object that can be used to chain
     * the calls
     */
    void setMAC(prop_id_t prop_id, const MAC& value);

    /**
     * Set the MAC address-vector-valued parameter to the specified value
     *
     * @param prop_id the property ID to set
     * @param value the value to set
     * @return a reference to this object that can be used to chain
     * the calls
     */
    void setMAC(prop_id_t prop_id, const std::vector<MAC>& value);

    /**
     * Add a value to a the specified unsigned 64-bit vector
     *
     * @param prop_id the property ID to set
     * @param value the value to set
     * @return a reference to this object that can be used to chain
     * the calls
     */
    void addUInt64(prop_id_t prop_id, uint64_t value);

    /**
     * Add a value to a the specified signed 64-bit vector
     *
     * @param prop_id the property ID to set
     * @param value the value to set
     * @return a reference to this object that can be used to chain
     * the calls
     */
    void addInt64(prop_id_t prop_id, int64_t value);

    /**
     * Add a value to a the specified string vector
     *
     * @param prop_id the property ID to set
     * @param value the value to set
     * @return a reference to this object that can be used to chain
     * the calls
     */
    void addString(prop_id_t prop_id, const std::string& value);

    /**
     * Add a value to a the specified reference vector
     *
     * @param prop_id the property ID to set
     * @param class_id the class_id of the reference
     * @param uri the URI of the reference
     * @return a reference to this object that can be used to chain
     * the calls
     */
    void addReference(prop_id_t prop_id,
                      class_id_t class_id, const URI& uri);

    /**
     * Add a value to a the specified MAC address vector
     *
     * @param prop_id the property ID to set
     * @param value the value to set
     * @return a reference to this object that can be used to chain
     * the calls
     */
    void addMAC(prop_id_t prop_id, const MAC& value);

private:
    class_id_t class_id;

    struct Value {
        PropertyInfo::property_type_t type;
        PropertyInfo::cardinality_t cardinality;
        boost::variant<boost::blank,
                       uint64_t,
                       int64_t,
                       std::string,
                       reference_t,
                       MAC,
                       std::vector<uint64_t>*,
                       std::vector<int64_t>*,
                       std::vector<std::string>*,
                       std::vector<reference_t>*,
                       std::vector<MAC>*> value;

        Value() {}
        Value(const Value& val);
        ~Value();
        Value& operator=(const Value& val);
    private:
        void clear();
    };

    typedef OF_UNORDERED_MAP<prop_key_t, Value> prop_map_t;
    prop_map_t prop_map;
    bool local;

    friend bool operator==(const ObjectInstance& lhs,
                           const ObjectInstance& rhs);
    friend bool operator!=(const ObjectInstance& lhs,
                           const ObjectInstance& rhs);
    friend bool operator==(const Value& lhs,
                           const Value& rhs);
    friend bool operator!=(const Value& lhs,
                           const Value& rhs);
    template <typename T>
    friend bool equal(const Value& lhs, const Value& rhs);
};

/**
 * Check for ObjectInstance equality.
 */
bool operator==(const ObjectInstance& lhs, const ObjectInstance& rhs);
/**
 * Check for ObjectInstance inequality.
 */
bool operator!=(const ObjectInstance& lhs, const ObjectInstance& rhs);

} /* namespace mointernal */
} /* namespace modb */
} /* namespace opflex */

#endif /* MODB_OBJECTINSTANCE_H_ */
