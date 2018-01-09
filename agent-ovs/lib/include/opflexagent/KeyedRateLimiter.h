/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for KeyedRateLimiter
 *
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_KEYED_RATE_LIMITER_H
#define OPFLEXAGENT_KEYED_RATE_LIMITER_H

#include <boost/noncopyable.hpp>

#include <string>
#include <unordered_set>
#include <mutex>
#include <vector>
#include <chrono>

namespace opflexagent {

/**
 * A class that helps to enforce that operations related to some
 * specific key can be performed only at some fixed rate.  The
 * BUCKET_TIME indicates how precise the timer will be.  The maximum
 * rate will be one in ((NBUCKETS-1) * BUCKET_TIME) time units, but
 * could be as little as one in (NBUCKETS * BUCKET_TIME) time units.
 *
 * @param K the key type; must be hashable
 * @param NBUCKETS the number of buckets for the rate limiter
 * @param BUCKET_TIME the amount of time to allow for each "tick" on
 * the buckets in milliseconds
 */
template <typename K,
          const size_t NBUCKETS,
          const uint64_t BUCKET_TIME>
class KeyedRateLimiter : private boost::noncopyable {
public:
    /**
     * Instantiate a keyed rate limiter with the specified number of
     * buckets and bucket duration.
     */
    KeyedRateLimiter()
        : buckets(NBUCKETS), curBucket(0),
          curBucketStart(std::chrono::steady_clock::now()) { }

    /**
     * Clear the rate limiter and reset its state to the initial state
     */
    void clear() {
        std::lock_guard<std::mutex> guard(mtx);
        do_clear();
    }

    /**
     * Apply the rate limiter to the given key.  Returns true if the
     * key has not occurred too recently
     *
     * @param key the key to check
     * @return true to indicate the event should be handled, otherwise
     * false.
     */
    bool event(K key) {
        time_point now = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> guard(mtx);

        duration increment(BUCKET_TIME);
        if (curBucketStart + increment * NBUCKETS < now) {
            // special case for very slow rate
            do_clear();
            buckets[curBucket].insert(key);
            return true;
        } else {
            // advance the timer
            while (curBucketStart + increment < now) {
                curBucketStart += increment;
                curBucket = (curBucket+1) % NBUCKETS;
                buckets[curBucket].clear();
            }
        }

        // check if any buckets in the window have the key
        for (key_set& ks : buckets) {
            if (ks.find(key) != ks.end())
                return false;
        }

        buckets[curBucket].insert(key);
        return true;
    }

private:
    typedef std::chrono::steady_clock::time_point time_point;
    typedef std::chrono::milliseconds duration;
    typedef std::unordered_set<K> key_set;

    std::mutex mtx;
    std::vector<key_set> buckets;
    size_t curBucket;
    time_point curBucketStart;

    void do_clear() {
        for (key_set& ks : buckets)
            ks.clear();
        curBucket = 0;
        curBucketStart = std::chrono::steady_clock::now();
    }
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_KEYED_RATE_LIMITER */
