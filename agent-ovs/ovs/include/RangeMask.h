/*
 * Definition of RangeMask class
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_RANGEMASK_H_
#define OPFLEXAGENT_RANGEMASK_H_

#include <stdint.h>
#include <ostream>
#include <vector>

#include <boost/optional.hpp>

namespace opflexagent {

/*
 * A masked value; the first part represents the base value and
 * the second part indicates bits in that value which are
 * ignored (i.e. set to zero in the mask).
 */
typedef std::pair<uint16_t, uint16_t> Mask;

/*
 * List of masked values.
 */
typedef std::vector<Mask> MaskList;

/**
 * Class to convert an integer range to a set of masked values.
 */
class RangeMask {
public:
    /**
     * Get the list of masked values that represent an integer range.
     * Both the start and end of the range are inclusive.
     *
     * @param start Beginning of the range; optional
     * @param end End of the range; optional
     * @param out List of masked-values for the range
     */
    static void getMasks(const boost::optional<uint16_t>& start,
                         const boost::optional<uint16_t>& end,
                         MaskList& out);
};

/**
 * Write a list of masked value to an output stream.
 */
std::ostream& operator<<(std::ostream& os, const MaskList& m);

}   // namespace opflexagent

#endif // OPFLEXAGENT_RANGEMASK_H_

