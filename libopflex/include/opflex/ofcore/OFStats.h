/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OFStats.h
 * @brief Interface definition file for OFFramework
 */
/*
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#ifndef OPFLEX_OFSTATS_H
#define OPFLEX_OFSTATS_H

#include <atomic>

class OFStats {

public:

    /**
     * Create a new framework instance
     */
    OFStats() {};

    /**
     * Destroy the framework instance
     */
    virtual ~OFStats() {};

    uint64_t getIdentReqs() { return identReqs; }
    void incrIdentReqs() { identReqs++; }
    uint64_t getIdentResps() { return identResps; }
    void incrIdentResps() { identResps++; }
    uint64_t getIdentErrs() { return identErrs; }
    void incrIdentErrs() { identErrs++; }

    uint64_t getPolResolves() { return polResolves; }
    void incrPolResolves() { polResolves++; }
    uint64_t getPolResolveResps() { return polResolveResps; }
    void incrPolResolveResps() { polResolveResps++; }
    uint64_t getPolResolveErrs() { return polResolveErrs; }
    void incrPolResolveErrs() { polResolveErrs++; }

    uint64_t getPolUnresolves() { return polUnresolves; }
    void incrPolUnresolves() { polUnresolves++; }
    uint64_t getPolUnresolveResps() { return polUnresolveResps; }
    void incrPolUnresolveResps() { polUnresolveResps++; }
    uint64_t getPolUnresolveErrs() { return polUnresolveErrs; }
    void incrPolUnresolveErrs() { polUnresolveErrs++; }

    uint64_t getPolUpdates() { return polUpdates; }
    void incrPolUpdates() { polUpdates++; }

    uint64_t getEpDeclares() { return epDeclares; }
    void incrEpDeclares() { epDeclares++; }
    uint64_t getEpDeclareResps() { return epDeclareResps; }
    void incrEpDeclareResps() { epDeclareResps++; }
    uint64_t getEpDeclareErrs() { return epDeclareErrs; }
    void incrEpDeclareErrs() { epDeclareErrs++; }

    uint64_t getEpUndeclares() { return epUndeclares; }
    void incrEpUndeclares() { epUndeclares++; }
    uint64_t getEpUndeclareResps() { return epUndeclareResps; }
    void incrEpUndeclareResps() { epUndeclareResps++; }
    uint64_t getEpUndeclareErrs() { return epUndeclareErrs; }
    void incrEpUndeclareErrs() { epUndeclareErrs++; }

    uint64_t getStateReports() { return stateReports; }
    void incrStateReports() { stateReports++; }
    uint64_t getStateReportResps() { return stateReportResps; }
    void incrStateReportResps() { stateReportResps++; }
    uint64_t getStateReportErrs() { return stateReportErrs; }
    void incrStateReportErrs() { stateReportErrs++; }

private:

    std::atomic_ullong identReqs{};
    std::atomic_ullong identResps{};
    std::atomic_ullong identErrs{};

    std::atomic_ullong polResolves{};
    std::atomic_ullong polResolveResps{};
    std::atomic_ullong polResolveErrs{};

    std::atomic_ullong polUnresolves{};
    std::atomic_ullong polUnresolveResps{};
    std::atomic_ullong polUnresolveErrs{};

    std::atomic_ullong polUpdates{};

    std::atomic_ullong epDeclares{};
    std::atomic_ullong epDeclareResps{};
    std::atomic_ullong epDeclareErrs{};

    std::atomic_ullong epUndeclares{};
    std::atomic_ullong epUndeclareResps{};
    std::atomic_ullong epUndeclareErrs{};

    std::atomic_ullong stateReports{};
    std::atomic_ullong stateReportResps{};
    std::atomic_ullong stateReportErrs{};
};

#endif //OPFLEX_OFSTATS_H
