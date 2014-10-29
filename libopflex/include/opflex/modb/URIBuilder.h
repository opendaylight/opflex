/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file URIBuilder.h
 * @brief Interface definition file for URI Builder
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEX_MODB_URIBUILDER_H
#define OPFLEX_MODB_URIBUILDER_H

#include <string>

#include "opflex/modb/URI.h"
#include "opflex/modb/MAC.h"

namespace opflex {
namespace modb {

/**
 * \addtogroup cpp
 * @{
 * \addtogroup modb
 * @{
 */

/**
 * @brief Build a URI using path elements from the root of the tree.
 */
class URIBuilder {
public:
    /**
     * Construct an empty URI builder representing the root element
     */
    URIBuilder();

    /**
     * Construct a URI builder that will append URI elements to the
     * specified URI
     * @param uri the URI to build from
     */
    URIBuilder(const URI& uri);

    /**
     * Destroy the URI Builder
     */
    ~URIBuilder();

    /**
     * Add a string-valued path element to the URI path, and
     * URI-escape the value.
     *
     * @param elementValue the value of the element
     */
    URIBuilder& addElement(const std::string& elementValue);

    /**
     * Add an unsigned int-valued path element to the URI path.
     *
     * @param elementValue the value of the element
     */
    URIBuilder& addElement(uint32_t elementValue);

    /**
     * Add a signed int-valued path element to the URI path.
     *
     * @param elementValue the value of the element
     */
    URIBuilder& addElement(int32_t elementValue);

    /**
     * Add an unsigned int-valued path element to the URI path.
     *
     * @param elementValue the value of the element
     */
    URIBuilder& addElement(uint64_t elementValue);

    /**
     * Add a signed int-valued path element to the URI path.
     *
     * @param elementValue the value of the element
     */
    URIBuilder& addElement(int64_t elementValue);

    /**
     * Add a mac-address-valued path element to the URI path.
     *
     * @param elementValue the value of the element
     */
    URIBuilder& addElement(const MAC& elementValue);

    /**
     * Build the URI from the path elements and return it
     */
    URI build();

private:
    class URIBuilderImpl;
    friend class URIBuilderImpl;
    URIBuilderImpl* pimpl;  
};
    
/* @} modb */
/* @} cpp */

} /* namespace modb */
} /* namespace opflex */

#endif /* OPFLEX_MODB_URIBUILDER_H */
