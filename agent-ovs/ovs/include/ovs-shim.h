/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* Shim layer that allows compiling against OVS libraries from C++.
   Some OVS headers use features allowed in C but not in C++, so
   there's no way to include them from C++ code. */

#ifndef OPFLEXAGENT_OVS_H_
#define OPFLEXAGENT_OVS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

    struct ofputil_flow_stats;
    struct ofputil_group_mod;
    struct ofputil_phy_port;
    struct mf_field;
    struct ofpact;
    struct ds;
    struct ofpbuf;
    struct dp_packet;
    struct flow;

    /**
     * Check if group mod is equal
     */
    int group_mod_equal(struct ofputil_group_mod* lgm,
                        struct ofputil_group_mod* rgm);

    /**
     * Check for action equality (ofpact_equal)
     */
    int action_equal(const struct ofpact* lhs, size_t lhs_len,
                     const struct ofpact* rhs, size_t rhs_len);

    /**
     * Call ofpacts_format
     */
    void format_action(const struct ofpact*, size_t ofpacts_len, struct ds*);

    /**
     * Call OVS's byte-order swap
     */
    uint64_t ovs_htonll(uint64_t v);

    /**
     * Call OVS's byte-order swap
     */
    uint64_t ovs_ntohll(uint64_t v);

    /**
     * Overwrite the raw field in each action
     */
    void override_raw_actions(const struct ofpact* acts, size_t len);

    /**
     * Overwrite the flow_has_vlan field where appropriate
     */
    void override_flow_has_vlan(const struct ofpact* acts, size_t len);

    /**
     * Duplicates forward declaration inside of ofp-actions.h
     */
    struct ofpact_set_field *ofpact_put_reg_load(struct ofpbuf *ofpacts,
                                                 const struct mf_field *,
                                                 const void *value,
                                                 const void *mask);

    /**
     * Move a register
     */
    void act_reg_move(struct ofpbuf* buf,
                      int srcRegId, int dstRegId,
                      uint8_t sourceOffset, uint8_t destOffset,
                      uint8_t nBits);

    /**
     * Load a register
     */
    void act_reg_load(struct ofpbuf* buf,
                      int regId, const void* regValue, const void* mask);

    /**
     * Set metadata
     */
    void act_metadata(struct ofpbuf* buf,
                      uint64_t metadata, uint64_t mask);

    /**
     * Set eth src
     */
    void act_eth_src(struct ofpbuf* buf, const uint8_t *srcMac,
                     int flowHasVlan);

    /**
     * set eth dst
     */
    void act_eth_dst(struct ofpbuf* buf, const uint8_t *dstMac,
                     int flowHasVlan);

    /**
     * set ip source v4
     */
    void act_ip_src_v4(struct ofpbuf* buf, uint32_t addr);

    /**
     * set ip source v6
     */
    void act_ip_src_v6(struct ofpbuf* buf, uint8_t* addr);

    /**
     * set ip dst v4
     */
    void act_ip_dst_v4(struct ofpbuf* buf, uint32_t addr);

    /**
     * set ip source v6
     */
    void act_ip_dst_v6(struct ofpbuf* buf, uint8_t* addr);

    /**
     * set l4 source port
     */
    void act_l4_src(struct ofpbuf* buf, uint16_t port, uint8_t proto);

    /**
     * set l4 destination port
     */
    void act_l4_dst(struct ofpbuf* buf, uint16_t port, uint8_t proto);

    /**
     * dec ttl
     */
    void act_decttl(struct ofpbuf* buf);

    /**
     * go to table
     */
    void act_go(struct ofpbuf* buf, uint8_t tableId);

    /**
     * resubmit
     */
    void act_resubmit(struct ofpbuf* buf, uint32_t inPort, uint8_t tableId);

    /**
     * output to port
     */
    void act_output(struct ofpbuf* buf, uint32_t port);

    /**
     * output to register value
     */
    void act_output_reg(struct ofpbuf* buf, int srcRegId);

    /**
     * output to group
     */
    void act_group(struct ofpbuf* buf, uint32_t groupId);

    /**
     * output to controller
     */
    void act_controller(struct ofpbuf* buf, uint16_t max_len);

    /**
     * push vlan
     */
    void act_push_vlan(struct ofpbuf* buf);

    /**
     * set vlan vid
     */
    void act_set_vlan_vid(struct ofpbuf* buf, uint16_t vlan);

    /**
     * pop vlan
     */
    void act_pop_vlan(struct ofpbuf* buf);

    /**
     * nat
     */
    void act_nat(struct ofpbuf* buf,
                 uint32_t ip_min, /* network order */
                 uint32_t ip_max, /* network order */
                 uint16_t proto_min,
                 uint16_t proto_max,
                 bool snat);
    /**
     * unnat
     */
    void act_unnat(struct ofpbuf* buf);

    /**
     * conntrack
     */
    void act_conntrack(struct ofpbuf* buf,
                       uint16_t flags,
                       uint16_t zoneImm,
                       int zoneSrc,
                       uint8_t recircTable,
                       uint16_t alg,
                       struct ofpact* actions,
                       size_t actsLen);

    /**
     * multipath
     */
    void act_multipath(struct ofpbuf* buf,
                       int fields,
                       uint16_t basis,
                       int algorithm,
                       uint16_t maxLink,
                       uint32_t arg,
                       int dst);

    /**
     * MAC/VLAN learn
     */
    void act_macvlan_learn(struct ofpbuf* ofpacts,
                           uint16_t prio,
                           uint64_t cookie,
                           uint8_t table);

    /**
     * Get the value of the output reg action
     */
    uint32_t get_output_reg_value(const struct ofpact* ofpacts,
                                  size_t ofpacts_len);

    /**
     * malloc a dp_packet
     */
    struct dp_packet* alloc_dpp();

    /**
     * dp_packet_use_const
     */
    void dp_packet_use_const(struct dp_packet *, const void *, size_t);

    /**
     * dp_packet_l4_size
     */
    size_t dpp_l4_size(const struct dp_packet *);

    /**
     * dp_packet_l4
     */
    void *dpp_l4(const struct dp_packet *);

    /**
     * dp_packet_l3
     */
    void *dpp_l3(const struct dp_packet *);

    /**
     * dp_packet_l2
     */
    void *dpp_l2(const struct dp_packet *);

    /**
     * dp_packet_data
     */
    void *dpp_data(const struct dp_packet *);

    /**
     * dp_packet_size
     */
    uint32_t dpp_size(const struct dp_packet *);

    /**
     * flow_extract
     */
    void flow_extract(struct dp_packet *, struct flow *);

    /**
     * malloc and init a dynamic string
     */
    struct ds* alloc_ds();

    /**
     * Clean up a dynamic string
     */
    void clean_ds(void* ds);

    struct ofputil_phy_port* alloc_phy_port();

#ifdef __cplusplus
}

#include <memory>

/**
 * Generic allocation and deletion functor
 */
template <typename P, P*(*Alloc)(), void(*Clean)(void*) = free>
class OvsPFunctor {
public:
    P* operator()() {
        return Alloc();
    }

    void operator()(void *p) {
        Clean(p);
    }
};

/**
 * Smart pointer type to hold a pointer to OVS types and properly
 * clean them up while allowing incomplete types
 */
template <typename P, typename Functor>
class OvsP : public std::unique_ptr<P, Functor> {
public:
    OvsP() : std::unique_ptr<P, Functor>(Functor()()) {}
    explicit OvsP(P* p) : std::unique_ptr<P, Functor>(p) {}
};

typedef OvsP<struct dp_packet,
             OvsPFunctor<struct dp_packet, alloc_dpp>> DpPacketP;
typedef OvsP<struct ds,
             OvsPFunctor<struct ds, alloc_ds, clean_ds>> DsP;
typedef OvsP<struct ofputil_phy_port,
             OvsPFunctor<struct ofputil_phy_port, alloc_phy_port>> PhyPortP;

#endif

#endif /* OPFLEXAGENT_OVS_H_ */
