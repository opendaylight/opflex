/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OFConstants.h
 * @brief Constants definition for the Opflex framework
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEX_CORE_TYPES_H
#define OPFLEX_CORE_TYPES_H

#if __cplusplus > 199711L && !defined(OF_NO_STD_CXX11)
#define OF_USE_STD_CXX11
#endif

#ifdef OF_USE_STD_CXX11
#include <unordered_set>
#include <unordered_map>
#include <memory>
#else
// Boost types are required for C++03, while we use standard library
// types for C++11 or later.
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#endif

/**
 * \addtogroup ofcore
 * @{
 */

namespace opflex {
namespace ofcore {

#ifdef OF_USE_STD_CXX11

/**
 * An unordered set type
 */
#define OF_UNORDERED_SET std::unordered_set

/**
 * An unordered map type
 */
#define OF_UNORDERED_MAP std::unordered_map

/**
 * A shared pointer type
 */
#define OF_SHARED_PTR std::shared_ptr

/**
 * A make shared functor
 */
#define OF_MAKE_SHARED std::make_shared

#else /* OF_USE_STD_CXX11 */

/**
 * An unordered set type
 */
#define OF_UNORDERED_SET boost::unordered_set

/**
 * An unordered map type
 */
#define OF_UNORDERED_MAP boost::unordered_map

/**
 * A shared pointer type
 */
#define OF_SHARED_PTR boost::shared_ptr

/**
 * A make shared functor
 */
#define OF_MAKE_SHARED boost::make_shared

#endif /* OF_USE_STD_CXX11 */

} /* namespace ofcore */
} /* namespace opflex */

/** @} ofcore */

#endif /* OPFLEX_CORE_TYPES_H */
