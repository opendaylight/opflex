/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file MAC.h
 * @brief Interface definition file for MACs
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef MODB_MAC_H
#define MODB_MAC_H

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
 * @brief A MAC address is used to identify devices on an ethernet
 * network.
 *
 * The string representation is a set of 6 hex-encoded bytes separated
 * by colon characters.
 */
class MAC {
public:
    /**
     * Construct a MAC consisting of all zeroes
     */
    MAC();

    /**
     * Construct a MAC using the given string representation
     *
     * @param mac the string representation of the mac
     * @throws std::invalid_argument if the MAC address is not valid
     */
    explicit MAC(const std::string& mac);

    /**
     * Construct a MAC using an array of 6 bytes, in network byte
     * order
     *
     * @param mac the mac represented as an array of 6 bytes
     */
    explicit MAC(uint8_t mac[6]);

    /**
     * Destroy the MAC
     */
    ~MAC();

    /**
     * Get the MAC represented as a string.
     */
    std::string toString() const;

    /**
     * Get the MAC represented as an array of 6 bytes in network byte order.
     */
    void toUIntArray(/* out */uint8_t mac[6]) const;

private:
    /**
     * Construct a MAC using a uint64_t, which must be in host byte
     * order, so that a constant such as 0x112233445566llu will
     * correspond to 11:22:33:44:55:66.
     *
     * @param mac the mac address
     */
    explicit MAC(uint64_t mac);

    /**
     * The MAC address, which will be stored in network byte order
     * using the leftmost 6 bytes
     */
    uint64_t mac;

    friend bool operator==(const MAC& lhs, const MAC& rhs);
    friend bool operator!=(const MAC& lhs, const MAC& rhs);
    friend size_t hash_value(MAC const& mac);
};

/**
 * Stream insertion operator
 */
std::ostream & operator<<(std::ostream &os, const MAC& mac);

/**
 * Check for MAC equality.
 */
bool operator==(const MAC& lhs, const MAC& rhs);
/**
 * Check for MAC inequality.
 */
bool operator!=(const MAC& lhs, const MAC& rhs);
/**
 * Compute a hash value for the MAC, making MAC suitable as a key in
 * a boost::unordered_map
 */
size_t hash_value(MAC const& mac);

/* @} modb */
/* @} cpp */

} /* namespace modb */
} /* namespace opflex */

#if __cplusplus > 199711L

namespace std {
/**
 * Template specialization for std::hash<opflex::modb::MAC>, making
 * MAC suitable as a key in a std::unordered_map
 */
template<> struct hash<opflex::modb::MAC> {
    /**
     * Hash the opflex::modb::MAC
     */
    std::size_t operator()(const opflex::modb::MAC& m) const {
        return opflex::modb::hash_value(m);
    }
};
} /* namespace std */

#endif

#endif /* MODB_MAC_H */
