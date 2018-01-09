/*
 * Implementation of RangeMask class
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <algorithm>
#include <iomanip>

#include "RangeMask.h"
#include <opflexagent/logging.h>

using namespace std;
using namespace boost;

namespace opflexagent {

/**
 * Checks if bit at particular position is set in given value.
 *
 * @param value Number to check
 * @param pos Bit position to check (position of LSB is 0)
 * @return true if bit is set
 */
static
bool isBitSet(uint16_t value, int pos) {
    return (value & (0x1 << pos)) != 0;
}

/**
 * Set bit at particular position in given value to 1 or 0.
 *
 * @param value Number to change
 * @param pos Bit position to change (position of LSB is 0)
 * @param on If true, the bit will be set to 1, else 0.
 * @return the changed value
 */
static
uint16_t setBit(uint16_t value, int pos, bool on) {
    if (on) {
        return (value | (0x1 << pos));
    } else {
        return (value & ~(0x1 << pos));
    }
}

/**
 * Construct a masked value from given base value. The generated
 * mask ignores all bit from the specified position to LSB.
 *
 * @param base Base value to use for the mask
 * @param pos Bit position from where to start ignoring (position of LSB is 0)
 * @return the masked value
 */
static
Mask createMask(uint16_t base, int pos) {
    uint16_t m = 0xffff;
    if (pos >= 0) {
        m = m << (pos+1);
    }
    return Mask(base & m, m);
}

void RangeMask::getMasks(const optional<uint16_t>& startOpt,
                         const optional<uint16_t>& endOpt,
                         MaskList& out) {
    out.clear();
    if (!startOpt && !endOpt) {
        return;
    }
    if (startOpt && !endOpt) {
        out.push_back(createMask(startOpt.get(), -1));
        return;
    }
    if (!startOpt && endOpt) {
        out.push_back(createMask(endOpt.get(), -1));
        return;
    }

    uint16_t start = startOpt.get();
    uint16_t end = endOpt.get();
    if (start > end) {
        swap(start, end);
    }

    if (start == end) {
        out.push_back(createMask(start, -1));
    } else {
        /* Find first bit from MSB that is different between 'start' & 'end' */
        int l2 = 15;
        while (l2 > 0 && isBitSet(start, l2) == isBitSet(end, l2)) {
            --l2;
        }
        /* Find first from LSB that is set in 'start' */
        int l1 = 0;
        while (l1 < l2 && !isBitSet(start, l1)) {
            ++l1;
        }
        /* Find beginning of trailing 1s in 'end' */
        int lto = -1;
        while (lto < l2 && isBitSet(end, lto+1)) {
            ++lto;
        }

        if (l1 == l2 && l2 == lto) {
            /* Special case:
             *    start = xxxx0..0
             *    end   = xxxx1..1
             */
            out.push_back(createMask(start, l2));
        } else {
            out.push_back(createMask(start, l1-1));
            int p = l1 + 1;
            while (p < l2) {
                if (!isBitSet(start, p)) {
                    out.push_back(createMask(setBit(start, p, true), p-1));
                }
                ++p;
            }
            p = l2 - 1;
            while (p > lto) {
                if (isBitSet(end, p)) {
                    out.push_back(createMask(setBit(end, p, false), p-1));
                }
                --p;
            }
            out.push_back(createMask(end, lto == l2 ? lto-1 : lto));
        }
    }
}

ostream& operator<<(ostream& os, const MaskList& m) {
    for (size_t i = 0; i < m.size(); ++i) {
        const Mask& mk = m[i];
        os << "0x" << setfill('0') << setw(4) << std::hex << mk.first
            << "/0x" << setfill('0') << setw(4) << mk.second << ", ";
    }
    os << std::dec;
    return os;
}

}   // namespace opflexagent
