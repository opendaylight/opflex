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
        std::lock_guard<std::mutex> guard(portsMutex);
        return ports.find(name) != ports.end() ? ports[name] : OFPP_NONE;
    }

    virtual void setPort(const std::string& name, uint32_t of_port_no) {
        std::lock_guard<std::mutex> guard(portsMutex);
        ports[name] = of_port_no;
    }

    virtual void erasePort(const std::string& port) {
        std::lock_guard<std::mutex> guard(portsMutex);
        ports.erase(port);
    }

    virtual const std::string& FindPort(uint32_t of_port_no) {
        std::lock_guard<std::mutex> guard(portsMutex);
        return RPortMap.at(of_port_no);
    }

    virtual void setPort(int of_port_no, const std::string& port) {
        std::lock_guard<std::mutex> guard(portsMutex);
        RPortMap[of_port_no] = port;
    }

private:
    std::unordered_map<std::string, uint32_t> ports;
    std::unordered_map<uint32_t, std::string> RPortMap;
    std::mutex portsMutex;
};

} // namespace opflexagent

#endif /* OPFLEXAGENT_TEST_MOCKPORTMAPPER_H_ */
