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

/**
 * OpFlex client stats counters
 */
class OFStats {

public:

    /**
     * Create a new instance
     */
    OFStats() {};

    /**
     * Destroy the instance
     */
    virtual ~OFStats() {};

    /** get the number of identity requests sent */
    uint64_t getIdentReqs() { return identReqs; }
    /** increment the number of identity requests sent */
    void incrIdentReqs() { identReqs++; }
    /** the number of identity responses received */
    uint64_t getIdentResps() { return identResps; }
    /** increment the number of identity responses received */
    void incrIdentResps() { identResps++; }
    /** get the number of errors received in identity responses */
    uint64_t getIdentErrs() { return identErrs; }
    /** increment the number of errors received in identity responses */
    void incrIdentErrs() { identErrs++; }

    /** get the number of policy_resolve msgs sent */
    uint64_t getPolResolves() { return polResolves; }
    /** increment the number of policy_resolve msgs sent */
    void incrPolResolves() { polResolves++; }
    /** get the number of policy_resolve responses received */
    uint64_t getPolResolveResps() { return polResolveResps; }
    /** increment the number of policy_resolve responses received */
    void incrPolResolveResps() { polResolveResps++; }
    /** get the number of errors receieved in policy_resolve responses */
    uint64_t getPolResolveErrs() { return polResolveErrs; }
    /** increment the number of errors receieved in policy_resolve responses */
    void incrPolResolveErrs() { polResolveErrs++; }

    /** get the number of policy_unresolve msgs sent */
    uint64_t getPolUnresolves() { return polUnresolves; }
    /** increment the number of policy_unresolve msgs sent */
    void incrPolUnresolves() { polUnresolves++; }
    /** get the number of policy_unresolve responses received */
    uint64_t getPolUnresolveResps() { return polUnresolveResps; }
    /** increment the number of policy_unresolve responses received */
    void incrPolUnresolveResps() { polUnresolveResps++; }
    /** get the number of errors receieved in policy_unresolve responses */
    uint64_t getPolUnresolveErrs() { return polUnresolveErrs; }
    /** increment the number of errors receieved in policy_unresolve responses */
    void incrPolUnresolveErrs() { polUnresolveErrs++; }

    /** get the number of policy updates received */
    uint64_t getPolUpdates() { return polUpdates; }
    /** increment the number of policy updates received */
    void incrPolUpdates() { polUpdates++; }

    /** get the number of endpoint_declare msgs sent */
    uint64_t getEpDeclares() { return epDeclares; }
    /** increment the number of endpoint_declare msgs sent */
    void incrEpDeclares() { epDeclares++; }
    /** get the number of endpoint_declare responses received */
    uint64_t getEpDeclareResps() { return epDeclareResps; }
    /** increment the number of endpoint_declare responses received */
    void incrEpDeclareResps() { epDeclareResps++; }
    /** get the number of errors receieved in endpoint_declare responses */
    uint64_t getEpDeclareErrs() { return epDeclareErrs; }
    /** increment the number of errors receieved in endpoint_declare responses */
    void incrEpDeclareErrs() { epDeclareErrs++; }

    /** get the number of endpoint_undeclare msgs sent */
    uint64_t getEpUndeclares() { return epUndeclares; }
    /** increment the number of endpoint_undeclare msgs sent */
    void incrEpUndeclares() { epUndeclares++; }
    /** get the number of endpoint_undeclare responses received */
    uint64_t getEpUndeclareResps() { return epUndeclareResps; }
    /** increment the number of endpoint_undeclare responses received */
    void incrEpUndeclareResps() { epUndeclareResps++; }
    /** get the number of errors receieved in endpoint_undeclare responses */
    uint64_t getEpUndeclareErrs() { return epUndeclareErrs; }
    /** increment the number of errors receieved in endpoint_undeclare responses */
    void incrEpUndeclareErrs() { epUndeclareErrs++; }

    /** get the number of state_report msgs sent */
    uint64_t getStateReports() { return stateReports; }
    /** increment the number of state_report msgs sent */
    void incrStateReports() { stateReports++; }
    /** get the number of state_report responses received */
    uint64_t getStateReportResps() { return stateReportResps; }
    /** increment the number of state_report responses received */
    void incrStateReportResps() { stateReportResps++; }
    /** get the number of errors receieved in state_report responses */
    uint64_t getStateReportErrs() { return stateReportErrs; }
    /** increment the number of errors receieved in state_report responses */
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
