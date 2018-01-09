/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_TEST_MOCKPORTMAPPER_H_
#define OPFLEXAGENT_TEST_MOCKPORTMAPPER_H_

#include "ovs-ofputil.h"
#include "PortMapper.h"

namespace opflexagent {

/**
 * Mock port mapper object useful for tests
 */
class MockPortMapper : public PortMapper {
public:
    virtual uint32_t FindPort(const std::string& name) {
        return ports.find(name) != ports.end() ? ports[name] : OFPP_NONE;
    }
    virtual const std::string& FindPort(uint32_t of_port_no) {
        return RPortMap.at(of_port_no);
    }

    std::unordered_map<std::string, uint32_t> ports;
    std::unordered_map<uint32_t, std::string> RPortMap;
};

} // namespace opflexagent

#endif /* OPFLEXAGENT_TEST_MOCKPORTMAPPER_H_ */
