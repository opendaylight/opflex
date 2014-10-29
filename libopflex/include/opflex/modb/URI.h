/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file URI.h
 * @brief Interface definition file for URIs
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef MODB_URI_H
#define MODB_URI_H

#include <boost/shared_ptr.hpp>
#include <boost/any.hpp>
#include <boost/functional/hash.hpp>
#include <string>
#include <vector>

namespace opflex {
namespace modb {

/**
 * \addtogroup cpp
 * @{
 * \addtogroup modb
 * @{
 */

/**
 * @brief A URI is used to identify managed objects in the MODB.
 *
 * It takes the form of a series of names and value of naming
 * properties such as "/childname1/5/childname2/8/value2" that
 * represents a unique path from the root of the tree to the specific
 * child.
 */
class URI {
public:
    /**
     * Construct a URI using the given string representation
     */
    explicit URI(const boost::shared_ptr<const std::string>& uri);

    /**
     * Construct a URI using the given string representation
     */
    explicit URI(const std::string& uri);

    /**
     * Construct a deep copy of the URI using the given URI
     */
    URI(const URI& uri);

    /**
     * Destroy the URI
     */
    ~URI();

    /**
     * Get the URI represented as a string.
     */
    const std::string& toString() const;

    /**
     * Parse the URI and get the unescaped path elements from the URI
     * @param elements an array that will receive the path elements
     */
    void getElements(/* out */ std::vector<std::string>& elements) const;

    /**
     * Assignment operator
     */
    URI& operator=( const URI& rhs );

    /**
     * Static root URI
     */
    static const URI ROOT;

private:
    boost::shared_ptr<const std::string> uri;
    size_t hashv;

    friend bool operator==(const URI& lhs, const URI& rhs);
    friend bool operator!=(const URI& lhs, const URI& rhs);
    friend size_t hash_value(URI const& uri);
};

/**
 * URI stream insertion
 */
std::ostream & operator<<(std::ostream &os, const URI& uri);

/**
 * Check for URI equality.
 */
bool operator==(const URI& lhs, const URI& rhs);
/**
 * Check for URI inequality.
 */
bool operator!=(const URI& lhs, const URI& rhs);
/**
 * Compute a hash value for the URI, making URI suitable as a key in
 * an unordered_map
 */
size_t hash_value(URI const& uri);

/* @} modb */
/* @} cpp */

} /* namespace modb */
} /* namespace opflex */

#endif /* MODB_URI_H */
