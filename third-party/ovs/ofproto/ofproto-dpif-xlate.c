/* Copyright (c) 2009, 2010, 2011, 2012, 2013, 2014 Nicira, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

#include <config.h>

#include "ofproto/ofproto-dpif-xlate.h"

#include <errno.h>

#include "bfd.h"
#include "bitmap.h"
#include "bond.h"
#include "bundle.h"
#include "byte-order.h"
#include "cfm.h"
#include "connmgr.h"
#include "coverage.h"
#include "dpif.h"
#include "dynamic-string.h"
#include "in-band.h"
#include "lacp.h"
#include "learn.h"
#include "list.h"
#include "mac-learning.h"
#include "meta-flow.h"
#include "multipath.h"
#include "netdev-vport.h"
#include "netlink.h"
#include "nx-match.h"
#include "odp-execute.h"
#include "ofp-actions.h"
#include "ofproto/ofproto-dpif-ipfix.h"
#include "ofproto/ofproto-dpif-mirror.h"
#include "ofproto/ofproto-dpif-monitor.h"
#include "ofproto/ofproto-dpif-sflow.h"
#include "ofproto/ofproto-dpif.h"
#include "ofproto/ofproto-provider.h"
#include "tunnel.h"
#include "vlog.h"

COVERAGE_DEFINE(xlate_actions);
COVERAGE_DEFINE(xlate_actions_oversize);
COVERAGE_DEFINE(xlate_actions_mpls_overflow);

VLOG_DEFINE_THIS_MODULE(ofproto_dpif_xlate);

/* Maximum depth of flow table recursion (due to resubmit actions) in a
 * flow translation. */
#define MAX_RESUBMIT_RECURSION 64
#define MAX_INTERNAL_RESUBMITS 1   /* Max resbmits allowed using rules in
                                      internal table. */

/* Maximum number of resubmit actions in a flow translation, whether they are
 * recursive or not. */
#define MAX_RESUBMITS (MAX_RESUBMIT_RECURSION * MAX_RESUBMIT_RECURSION)

struct xbridge {
    struct hmap_node hmap_node;   /* Node in global 'xbridges' map. */
    struct ofproto_dpif *ofproto; /* Key in global 'xbridges' map. */

    struct list xbundles;         /* Owned xbundles. */
    struct hmap xports;           /* Indexed by ofp_port. */

    char *name;                   /* Name used in log messages. */
    struct dpif *dpif;            /* Datapath interface. */
    struct mac_learning *ml;      /* Mac learning handle. */
    struct mbridge *mbridge;      /* Mirroring. */
    struct dpif_sflow *sflow;     /* SFlow handle, or null. */
    struct dpif_ipfix *ipfix;     /* Ipfix handle, or null. */
    struct netflow *netflow;      /* Netflow handle, or null. */
    struct stp *stp;              /* STP or null if disabled. */

    /* Special rules installed by ofproto-dpif. */
    struct rule_dpif *miss_rule;
    struct rule_dpif *no_packet_in_rule;

    enum ofp_config_flags frag;   /* Fragmentation handling. */
    bool has_in_band;             /* Bridge has in band control? */
    bool forward_bpdu;            /* Bridge forwards STP BPDUs? */

    /* True if the datapath supports recirculation. */
    bool enable_recirc;

    /* True if the datapath supports variable-length
     * OVS_USERSPACE_ATTR_USERDATA in OVS_ACTION_ATTR_USERSPACE actions.
     * False if the datapath supports only 8-byte (or shorter) userdata. */
    bool variable_length_userdata;

    /* Number of MPLS label stack entries that the datapath supports
     * in matches. */
    size_t max_mpls_depth;
};

struct xbundle {
    struct hmap_node hmap_node;    /* In global 'xbundles' map. */
    struct ofbundle *ofbundle;     /* Key in global 'xbundles' map. */

    struct list list_node;         /* In parent 'xbridges' list. */
    struct xbridge *xbridge;       /* Parent xbridge. */

    struct list xports;            /* Contains "struct xport"s. */

    char *name;                    /* Name used in log messages. */
    struct bond *bond;             /* Nonnull iff more than one port. */
    struct lacp *lacp;             /* LACP handle or null. */

    enum port_vlan_mode vlan_mode; /* VLAN mode. */
    int vlan;                      /* -1=trunk port, else a 12-bit VLAN ID. */
    unsigned long *trunks;         /* Bitmap of trunked VLANs, if 'vlan' == -1.
                                    * NULL if all VLANs are trunked. */
    bool use_priority_tags;        /* Use 802.1p tag for frames in VLAN 0? */
    bool floodable;                /* No port has OFPUTIL_PC_NO_FLOOD set? */
};

struct xport {
    struct hmap_node hmap_node;      /* Node in global 'xports' map. */
    struct ofport_dpif *ofport;      /* Key in global 'xports map. */

    struct hmap_node ofp_node;       /* Node in parent xbridge 'xports' map. */
    ofp_port_t ofp_port;             /* Key in parent xbridge 'xports' map. */

    odp_port_t odp_port;             /* Datapath port number or ODPP_NONE. */

    struct list bundle_node;         /* In parent xbundle (if it exists). */
    struct xbundle *xbundle;         /* Parent xbundle or null. */

    struct netdev *netdev;           /* 'ofport''s netdev. */

    struct xbridge *xbridge;         /* Parent bridge. */
    struct xport *peer;              /* Patch port peer or null. */

    enum ofputil_port_config config; /* OpenFlow port configuration. */
    enum ofputil_port_state state;   /* OpenFlow port state. */
    int stp_port_no;                 /* STP port number or -1 if not in use. */

    struct hmap skb_priorities;      /* Map of 'skb_priority_to_dscp's. */

    bool may_enable;                 /* May be enabled in bonds. */
    bool is_tunnel;                  /* Is a tunnel port. */

    struct cfm *cfm;                 /* CFM handle or null. */
    struct bfd *bfd;                 /* BFD handle or null. */
};

struct xlate_ctx {
    struct xlate_in *xin;
    struct xlate_out *xout;

    const struct xbridge *xbridge;

    /* Flow at the last commit. */
    struct flow base_flow;

    /* Tunnel IP destination address as received.  This is stored separately
     * as the base_flow.tunnel is cleared on init to reflect the datapath
     * behavior.  Used to make sure not to send tunneled output to ourselves,
     * which might lead to an infinite loop.  This could happen easily
     * if a tunnel is marked as 'ip_remote=flow', and the flow does not
     * actually set the tun_dst field. */
    ovs_be32 orig_tunnel_ip_dst;

    /* Stack for the push and pop actions.  Each stack element is of type
     * "union mf_subvalue". */
    union mf_subvalue init_stack[1024 / sizeof(union mf_subvalue)];
    struct ofpbuf stack;

    /* The rule that we are currently translating, or NULL. */
    struct rule_dpif *rule;

    /* Resubmit statistics, via xlate_table_action(). */
    int recurse;                /* Current resubmit nesting depth. */
    int resubmits;              /* Total number of resubmits. */
    bool in_group;              /* Currently translating ofgroup, if true. */

    uint32_t orig_skb_priority; /* Priority when packet arrived. */
    uint8_t table_id;           /* OpenFlow table ID where flow was found. */
    uint32_t sflow_n_outputs;   /* Number of output ports. */
    odp_port_t sflow_odp_port;  /* Output port for composing sFlow action. */
    uint16_t user_cookie_offset;/* Used for user_action_cookie fixup. */
    bool exit;                  /* No further actions should be processed. */

    bool use_recirc;            /* Should generate recirc? */
    struct xlate_recirc recirc; /* Information used for generating
                                 * recirculation actions */

    /* OpenFlow 1.1+ action set.
     *
     * 'action_set' accumulates "struct ofpact"s added by OFPACT_WRITE_ACTIONS.
     * When translation is otherwise complete, ofpacts_execute_action_set()
     * converts it to a set of "struct ofpact"s that can be translated into
     * datapath actions.   */
    struct ofpbuf action_set;   /* Action set. */
    uint64_t action_set_stub[1024 / 8];
};

/* A controller may use OFPP_NONE as the ingress port to indicate that
 * it did not arrive on a "real" port.  'ofpp_none_bundle' exists for
 * when an input bundle is needed for validation (e.g., mirroring or
 * OFPP_NORMAL processing).  It is not connected to an 'ofproto' or have
 * any 'port' structs, so care must be taken when dealing with it. */
static struct xbundle ofpp_none_bundle = {
    .name      = "OFPP_NONE",
    .vlan_mode = PORT_VLAN_TRUNK
};

/* Node in 'xport''s 'skb_priorities' map.  Used to maintain a map from
 * 'priority' (the datapath's term for QoS queue) to the dscp bits which all
 * traffic egressing the 'ofport' with that priority should be marked with. */
struct skb_priority_to_dscp {
    struct hmap_node hmap_node; /* Node in 'ofport_dpif''s 'skb_priorities'. */
    uint32_t skb_priority;      /* Priority of this queue (see struct flow). */

    uint8_t dscp;               /* DSCP bits to mark outgoing traffic with. */
};

enum xc_type {
    XC_RULE,
    XC_BOND,
    XC_NETDEV,
    XC_NETFLOW,
    XC_MIRROR,
    XC_LEARN,
    XC_NORMAL,
    XC_FIN_TIMEOUT,
    XC_GROUP,
};

/* xlate_cache entries hold enough information to perform the side effects of
 * xlate_actions() for a rule, without needing to perform rule translation
 * from scratch. The primary usage of these is to submit statistics to objects
 * that a flow relates to, although they may be used for other effects as well
 * (for instance, refreshing hard timeouts for learned flows). */
struct xc_entry {
    enum xc_type type;
    union {
        struct rule_dpif *rule;
        struct {
            struct netdev *tx;
            struct netdev *rx;
            struct bfd *bfd;
        } dev;
        struct {
            struct netflow *netflow;
            struct flow *flow;
            ofp_port_t iface;
        } nf;
        struct {
            struct mbridge *mbridge;
            mirror_mask_t mirrors;
        } mirror;
        struct {
            struct bond *bond;
            struct flow *flow;
            uint16_t vid;
        } bond;
        struct {
            struct ofproto_dpif *ofproto;
            struct ofputil_flow_mod *fm;
            struct ofpbuf *ofpacts;
        } learn;
        struct {
            struct ofproto_dpif *ofproto;
            struct flow *flow;
            int vlan;
        } normal;
        struct {
            struct rule_dpif *rule;
            uint16_t idle;
            uint16_t hard;
        } fin;
        struct {
            struct group_dpif *group;
            struct ofputil_bucket *bucket;
        } group;
    } u;
};

#define XC_ENTRY_FOR_EACH(entry, entries, xcache)               \
    entries = xcache->entries;                                  \
    for (entry = ofpbuf_try_pull(&entries, sizeof *entry);      \
         entry;                                                 \
         entry = ofpbuf_try_pull(&entries, sizeof *entry))

struct xlate_cache {
    struct ofpbuf entries;
};

/* Xlate config contains hash maps of all bridges, bundles and ports.
 * Xcfgp contains the pointer to the current xlate configuration.
 * When the main thread needs to change the configuration, it copies xcfgp to
 * new_xcfg and edits new_xcfg. This enables the use of RCU locking which
 * does not block handler and revalidator threads. */
struct xlate_cfg {
    struct hmap xbridges;
    struct hmap xbundles;
    struct hmap xports;
};
static OVSRCU_TYPE(struct xlate_cfg *) xcfgp = OVSRCU_TYPE_INITIALIZER;
static struct xlate_cfg *new_xcfg = NULL;

static bool may_receive(const struct xport *, struct xlate_ctx *);
static void do_xlate_actions(const struct ofpact *, size_t ofpacts_len,
                             struct xlate_ctx *);
static void xlate_normal(struct xlate_ctx *);
static void xlate_report(struct xlate_ctx *, const char *);
static void xlate_table_action(struct xlate_ctx *, ofp_port_t in_port,
                               uint8_t table_id, bool may_packet_in,
                               bool honor_table_miss);
static bool input_vid_is_valid(uint16_t vid, struct xbundle *, bool warn);
static uint16_t input_vid_to_vlan(const struct xbundle *, uint16_t vid);
static void output_normal(struct xlate_ctx *, const struct xbundle *,
                          uint16_t vlan);
static void compose_output_action(struct xlate_ctx *, ofp_port_t ofp_port);

static struct xbridge *xbridge_lookup(struct xlate_cfg *,
                                      const struct ofproto_dpif *);
static struct xbundle *xbundle_lookup(struct xlate_cfg *,
                                      const struct ofbundle *);
static struct xport *xport_lookup(struct xlate_cfg *,
                                  const struct ofport_dpif *);
static struct xport *get_ofp_port(const struct xbridge *, ofp_port_t ofp_port);
static struct skb_priority_to_dscp *get_skb_priority(const struct xport *,
                                                     uint32_t skb_priority);
static void clear_skb_priorities(struct xport *);
static bool dscp_from_skb_priority(const struct xport *, uint32_t skb_priority,
                                   uint8_t *dscp);

static struct xc_entry *xlate_cache_add_entry(struct xlate_cache *xc,
                                              enum xc_type type);
static void xlate_xbridge_init(struct xlate_cfg *, struct xbridge *);
static void xlate_xbundle_init(struct xlate_cfg *, struct xbundle *);
static void xlate_xport_init(struct xlate_cfg *, struct xport *);
static void xlate_xbridge_set(struct xbridge *xbridge,
                              struct dpif *dpif,
                              struct rule_dpif *miss_rule,
                              struct rule_dpif *no_packet_in_rule,
                              const struct mac_learning *ml, struct stp *stp,
                              const struct mbridge *mbridge,
                              const struct dpif_sflow *sflow,
                              const struct dpif_ipfix *ipfix,
                              const struct netflow *netflow,
                              enum ofp_config_flags frag,
                              bool forward_bpdu, bool has_in_band,
                              bool enable_recirc,
                              bool variable_length_userdata,
                              size_t max_mpls_depth);
static void xlate_xbundle_set(struct xbundle *xbundle,
                              enum port_vlan_mode vlan_mode, int vlan,
                              unsigned long *trunks, bool use_priority_tags,
                              const struct bond *bond, const struct lacp *lacp,
                              bool floodable);
static void xlate_xport_set(struct xport *xport, odp_port_t odp_port,
                            const struct netdev *netdev, const struct cfm *cfm,
                            const struct bfd *bfd, int stp_port_no,
                            enum ofputil_port_config config,
                            enum ofputil_port_state state, bool is_tunnel,
                            bool may_enable);
static void xlate_xbridge_remove(struct xlate_cfg *, struct xbridge *);
static void xlate_xbundle_remove(struct xlate_cfg *, struct xbundle *);
static void xlate_xport_remove(struct xlate_cfg *, struct xport *);
static void xlate_xbridge_copy(struct xbridge *);
static void xlate_xbundle_copy(struct xbridge *, struct xbundle *);
static void xlate_xport_copy(struct xbridge *, struct xbundle *,
                             struct xport *);
static void xlate_xcfg_free(struct xlate_cfg *);


static void
xlate_xbridge_init(struct xlate_cfg *xcfg, struct xbridge *xbridge)
{
    list_init(&xbridge->xbundles);
    hmap_init(&xbridge->xports);
    hmap_insert(&xcfg->xbridges, &xbridge->hmap_node,
                hash_pointer(xbridge->ofproto, 0));
}

static void
xlate_xbundle_init(struct xlate_cfg *xcfg, struct xbundle *xbundle)
{
    list_init(&xbundle->xports);
    list_insert(&xbundle->xbridge->xbundles, &xbundle->list_node);
    hmap_insert(&xcfg->xbundles, &xbundle->hmap_node,
                hash_pointer(xbundle->ofbundle, 0));
}

static void
xlate_xport_init(struct xlate_cfg *xcfg, struct xport *xport)
{
    hmap_init(&xport->skb_priorities);
    hmap_insert(&xcfg->xports, &xport->hmap_node,
                hash_pointer(xport->ofport, 0));
    hmap_insert(&xport->xbridge->xports, &xport->ofp_node,
                hash_ofp_port(xport->ofp_port));
}

static void
xlate_xbridge_set(struct xbridge *xbridge,
                  struct dpif *dpif,
                  struct rule_dpif *miss_rule,
                  struct rule_dpif *no_packet_in_rule,
                  const struct mac_learning *ml, struct stp *stp,
                  const struct mbridge *mbridge,
                  const struct dpif_sflow *sflow,
                  const struct dpif_ipfix *ipfix,
                  const struct netflow *netflow, enum ofp_config_flags frag,
                  bool forward_bpdu, bool has_in_band,
                  bool enable_recirc,
                  bool variable_length_userdata,
                  size_t max_mpls_depth)
{
    if (xbridge->ml != ml) {
        mac_learning_unref(xbridge->ml);
        xbridge->ml = mac_learning_ref(ml);
    }

    if (xbridge->mbridge != mbridge) {
        mbridge_unref(xbridge->mbridge);
        xbridge->mbridge = mbridge_ref(mbridge);
    }

    if (xbridge->sflow != sflow) {
        dpif_sflow_unref(xbridge->sflow);
        xbridge->sflow = dpif_sflow_ref(sflow);
    }

    if (xbridge->ipfix != ipfix) {
        dpif_ipfix_unref(xbridge->ipfix);
        xbridge->ipfix = dpif_ipfix_ref(ipfix);
    }

    if (xbridge->stp != stp) {
        stp_unref(xbridge->stp);
        xbridge->stp = stp_ref(stp);
    }

    if (xbridge->netflow != netflow) {
        netflow_unref(xbridge->netflow);
        xbridge->netflow = netflow_ref(netflow);
    }

    xbridge->dpif = dpif;
    xbridge->forward_bpdu = forward_bpdu;
    xbridge->has_in_band = has_in_band;
    xbridge->frag = frag;
    xbridge->miss_rule = miss_rule;
    xbridge->no_packet_in_rule = no_packet_in_rule;
    xbridge->enable_recirc = enable_recirc;
    xbridge->variable_length_userdata = variable_length_userdata;
    xbridge->max_mpls_depth = max_mpls_depth;
}

static void
xlate_xbundle_set(struct xbundle *xbundle,
                  enum port_vlan_mode vlan_mode, int vlan,
                  unsigned long *trunks, bool use_priority_tags,
                  const struct bond *bond, const struct lacp *lacp,
                  bool floodable)
{
    ovs_assert(xbundle->xbridge);

    xbundle->vlan_mode = vlan_mode;
    xbundle->vlan = vlan;
    xbundle->trunks = trunks;
    xbundle->use_priority_tags = use_priority_tags;
    xbundle->floodable = floodable;

    if (xbundle->bond != bond) {
        bond_unref(xbundle->bond);
        xbundle->bond = bond_ref(bond);
    }

    if (xbundle->lacp != lacp) {
        lacp_unref(xbundle->lacp);
        xbundle->lacp = lacp_ref(lacp);
    }
}

static void
xlate_xport_set(struct xport *xport, odp_port_t odp_port,
                const struct netdev *netdev, const struct cfm *cfm,
                const struct bfd *bfd, int stp_port_no,
                enum ofputil_port_config config, enum ofputil_port_state state,
                bool is_tunnel, bool may_enable)
{
    xport->config = config;
    xport->state = state;
    xport->stp_port_no = stp_port_no;
    xport->is_tunnel = is_tunnel;
    xport->may_enable = may_enable;
    xport->odp_port = odp_port;

    if (xport->cfm != cfm) {
        cfm_unref(xport->cfm);
        xport->cfm = cfm_ref(cfm);
    }

    if (xport->bfd != bfd) {
        bfd_unref(xport->bfd);
        xport->bfd = bfd_ref(bfd);
    }

    if (xport->netdev != netdev) {
        netdev_close(xport->netdev);
        xport->netdev = netdev_ref(netdev);
    }
}

static void
xlate_xbridge_copy(struct xbridge *xbridge)
{
    struct xbundle *xbundle;
    struct xport *xport;
    struct xbridge *new_xbridge = xzalloc(sizeof *xbridge);
    new_xbridge->ofproto = xbridge->ofproto;
    new_xbridge->name = xstrdup(xbridge->name);
    xlate_xbridge_init(new_xcfg, new_xbridge);

    xlate_xbridge_set(new_xbridge,
                      xbridge->dpif, xbridge->miss_rule,
                      xbridge->no_packet_in_rule, xbridge->ml, xbridge->stp,
                      xbridge->mbridge, xbridge->sflow, xbridge->ipfix,
                      xbridge->netflow, xbridge->frag, xbridge->forward_bpdu,
                      xbridge->has_in_band, xbridge->enable_recirc,
                      xbridge->variable_length_userdata,
                      xbridge->max_mpls_depth);
    LIST_FOR_EACH (xbundle, list_node, &xbridge->xbundles) {
        xlate_xbundle_copy(new_xbridge, xbundle);
    }

    /* Copy xports which are not part of a xbundle */
    HMAP_FOR_EACH (xport, ofp_node, &xbridge->xports) {
        if (!xport->xbundle) {
            xlate_xport_copy(new_xbridge, NULL, xport);
        }
    }
}

static void
xlate_xbundle_copy(struct xbridge *xbridge, struct xbundle *xbundle)
{
    struct xport *xport;
    struct xbundle *new_xbundle = xzalloc(sizeof *xbundle);
    new_xbundle->ofbundle = xbundle->ofbundle;
    new_xbundle->xbridge = xbridge;
    new_xbundle->name = xstrdup(xbundle->name);
    xlate_xbundle_init(new_xcfg, new_xbundle);

    xlate_xbundle_set(new_xbundle, xbundle->vlan_mode,
                      xbundle->vlan, xbundle->trunks,
                      xbundle->use_priority_tags, xbundle->bond, xbundle->lacp,
                      xbundle->floodable);
    LIST_FOR_EACH (xport, bundle_node, &xbundle->xports) {
        xlate_xport_copy(xbridge, new_xbundle, xport);
    }
}

static void
xlate_xport_copy(struct xbridge *xbridge, struct xbundle *xbundle,
                 struct xport *xport)
{
    struct skb_priority_to_dscp *pdscp, *new_pdscp;
    struct xport *new_xport = xzalloc(sizeof *xport);
    new_xport->ofport = xport->ofport;
    new_xport->ofp_port = xport->ofp_port;
    new_xport->xbridge = xbridge;
    xlate_xport_init(new_xcfg, new_xport);

    xlate_xport_set(new_xport, xport->odp_port, xport->netdev, xport->cfm,
                    xport->bfd, xport->stp_port_no, xport->config, xport->state,
                    xport->is_tunnel, xport->may_enable);

    if (xport->peer) {
        struct xport *peer = xport_lookup(new_xcfg, xport->peer->ofport);
        if (peer) {
            new_xport->peer = peer;
            new_xport->peer->peer = new_xport;
        }
    }

    if (xbundle) {
        new_xport->xbundle = xbundle;
        list_insert(&new_xport->xbundle->xports, &new_xport->bundle_node);
    }

    HMAP_FOR_EACH (pdscp, hmap_node, &xport->skb_priorities) {
        new_pdscp = xmalloc(sizeof *pdscp);
        new_pdscp->skb_priority = pdscp->skb_priority;
        new_pdscp->dscp = pdscp->dscp;
        hmap_insert(&new_xport->skb_priorities, &new_pdscp->hmap_node,
                    hash_int(new_pdscp->skb_priority, 0));
    }
}

/* Sets the current xlate configuration to new_xcfg and frees the old xlate
 * configuration in xcfgp.
 *
 * This needs to be called after editing the xlate configuration.
 *
 * Functions that edit the new xlate configuration are
 * xlate_<ofport/bundle/ofport>_set and xlate_<ofport/bundle/ofport>_remove.
 *
 * A sample workflow:
 *
 * xlate_txn_start();
 * ...
 * edit_xlate_configuration();
 * ...
 * xlate_txn_commit(); */
void
xlate_txn_commit(void)
{
    struct xlate_cfg *xcfg = ovsrcu_get(struct xlate_cfg *, &xcfgp);

    ovsrcu_set(&xcfgp, new_xcfg);
    ovsrcu_postpone(xlate_xcfg_free, xcfg);

    new_xcfg = NULL;
}

/* Copies the current xlate configuration in xcfgp to new_xcfg.
 *
 * This needs to be called prior to editing the xlate configuration. */
void
xlate_txn_start(void)
{
    struct xbridge *xbridge;
    struct xlate_cfg *xcfg;

    ovs_assert(!new_xcfg);

    new_xcfg = xmalloc(sizeof *new_xcfg);
    hmap_init(&new_xcfg->xbridges);
    hmap_init(&new_xcfg->xbundles);
    hmap_init(&new_xcfg->xports);

    xcfg = ovsrcu_get(struct xlate_cfg *, &xcfgp);
    if (!xcfg) {
        return;
    }

    HMAP_FOR_EACH (xbridge, hmap_node, &xcfg->xbridges) {
        xlate_xbridge_copy(xbridge);
    }
}


static void
xlate_xcfg_free(struct xlate_cfg *xcfg)
{
    struct xbridge *xbridge, *next_xbridge;

    if (!xcfg) {
        return;
    }

    HMAP_FOR_EACH_SAFE (xbridge, next_xbridge, hmap_node, &xcfg->xbridges) {
        xlate_xbridge_remove(xcfg, xbridge);
    }

    hmap_destroy(&xcfg->xbridges);
    hmap_destroy(&xcfg->xbundles);
    hmap_destroy(&xcfg->xports);
    free(xcfg);
}

void
xlate_ofproto_set(struct ofproto_dpif *ofproto, const char *name,
                  struct dpif *dpif, struct rule_dpif *miss_rule,
                  struct rule_dpif *no_packet_in_rule,
                  const struct mac_learning *ml, struct stp *stp,
                  const struct mbridge *mbridge,
                  const struct dpif_sflow *sflow,
                  const struct dpif_ipfix *ipfix,
                  const struct netflow *netflow, enum ofp_config_flags frag,
                  bool forward_bpdu, bool has_in_band,
                  bool enable_recirc,
                  bool variable_length_userdata,
                  size_t max_mpls_depth)
{
    struct xbridge *xbridge;

    ovs_assert(new_xcfg);

    xbridge = xbridge_lookup(new_xcfg, ofproto);
    if (!xbridge) {
        xbridge = xzalloc(sizeof *xbridge);
        xbridge->ofproto = ofproto;

        xlate_xbridge_init(new_xcfg, xbridge);
    }

    free(xbridge->name);
    xbridge->name = xstrdup(name);

    xlate_xbridge_set(xbridge, dpif, miss_rule, no_packet_in_rule, ml, stp,
                      mbridge, sflow, ipfix, netflow, frag, forward_bpdu,
                      has_in_band, enable_recirc, variable_length_userdata,
                      max_mpls_depth);
}

static void
xlate_xbridge_remove(struct xlate_cfg *xcfg, struct xbridge *xbridge)
{
    struct xbundle *xbundle, *next_xbundle;
    struct xport *xport, *next_xport;

    if (!xbridge) {
        return;
    }

    HMAP_FOR_EACH_SAFE (xport, next_xport, ofp_node, &xbridge->xports) {
        xlate_xport_remove(xcfg, xport);
    }

    LIST_FOR_EACH_SAFE (xbundle, next_xbundle, list_node, &xbridge->xbundles) {
        xlate_xbundle_remove(xcfg, xbundle);
    }

    hmap_remove(&xcfg->xbridges, &xbridge->hmap_node);
    mac_learning_unref(xbridge->ml);
    mbridge_unref(xbridge->mbridge);
    dpif_sflow_unref(xbridge->sflow);
    dpif_ipfix_unref(xbridge->ipfix);
    stp_unref(xbridge->stp);
    hmap_destroy(&xbridge->xports);
    free(xbridge->name);
    free(xbridge);
}

void
xlate_remove_ofproto(struct ofproto_dpif *ofproto)
{
    struct xbridge *xbridge;

    ovs_assert(new_xcfg);

    xbridge = xbridge_lookup(new_xcfg, ofproto);
    xlate_xbridge_remove(new_xcfg, xbridge);
}

void
xlate_bundle_set(struct ofproto_dpif *ofproto, struct ofbundle *ofbundle,
                 const char *name, enum port_vlan_mode vlan_mode, int vlan,
                 unsigned long *trunks, bool use_priority_tags,
                 const struct bond *bond, const struct lacp *lacp,
                 bool floodable)
{
    struct xbundle *xbundle;

    ovs_assert(new_xcfg);

    xbundle = xbundle_lookup(new_xcfg, ofbundle);
    if (!xbundle) {
        xbundle = xzalloc(sizeof *xbundle);
        xbundle->ofbundle = ofbundle;
        xbundle->xbridge = xbridge_lookup(new_xcfg, ofproto);

        xlate_xbundle_init(new_xcfg, xbundle);
    }

    free(xbundle->name);
    xbundle->name = xstrdup(name);

    xlate_xbundle_set(xbundle, vlan_mode, vlan, trunks,
                      use_priority_tags, bond, lacp, floodable);
}

static void
xlate_xbundle_remove(struct xlate_cfg *xcfg, struct xbundle *xbundle)
{
    struct xport *xport, *next;

    if (!xbundle) {
        return;
    }

    LIST_FOR_EACH_SAFE (xport, next, bundle_node, &xbundle->xports) {
        list_remove(&xport->bundle_node);
        xport->xbundle = NULL;
    }

    hmap_remove(&xcfg->xbundles, &xbundle->hmap_node);
    list_remove(&xbundle->list_node);
    bond_unref(xbundle->bond);
    lacp_unref(xbundle->lacp);
    free(xbundle->name);
    free(xbundle);
}

void
xlate_bundle_remove(struct ofbundle *ofbundle)
{
    struct xbundle *xbundle;

    ovs_assert(new_xcfg);

    xbundle = xbundle_lookup(new_xcfg, ofbundle);
    xlate_xbundle_remove(new_xcfg, xbundle);
}

void
xlate_ofport_set(struct ofproto_dpif *ofproto, struct ofbundle *ofbundle,
                 struct ofport_dpif *ofport, ofp_port_t ofp_port,
                 odp_port_t odp_port, const struct netdev *netdev,
                 const struct cfm *cfm, const struct bfd *bfd,
                 struct ofport_dpif *peer, int stp_port_no,
                 const struct ofproto_port_queue *qdscp_list, size_t n_qdscp,
                 enum ofputil_port_config config,
                 enum ofputil_port_state state, bool is_tunnel,
                 bool may_enable)
{
    size_t i;
    struct xport *xport;

    ovs_assert(new_xcfg);

    xport = xport_lookup(new_xcfg, ofport);
    if (!xport) {
        xport = xzalloc(sizeof *xport);
        xport->ofport = ofport;
        xport->xbridge = xbridge_lookup(new_xcfg, ofproto);
        xport->ofp_port = ofp_port;

        xlate_xport_init(new_xcfg, xport);
    }

    ovs_assert(xport->ofp_port == ofp_port);

    xlate_xport_set(xport, odp_port, netdev, cfm, bfd, stp_port_no, config,
                    state, is_tunnel, may_enable);

    if (xport->peer) {
        xport->peer->peer = NULL;
    }
    xport->peer = xport_lookup(new_xcfg, peer);
    if (xport->peer) {
        xport->peer->peer = xport;
    }

    if (xport->xbundle) {
        list_remove(&xport->bundle_node);
    }
    xport->xbundle = xbundle_lookup(new_xcfg, ofbundle);
    if (xport->xbundle) {
        list_insert(&xport->xbundle->xports, &xport->bundle_node);
    }

    clear_skb_priorities(xport);
    for (i = 0; i < n_qdscp; i++) {
        struct skb_priority_to_dscp *pdscp;
        uint32_t skb_priority;

        if (dpif_queue_to_priority(xport->xbridge->dpif, qdscp_list[i].queue,
                                   &skb_priority)) {
            continue;
        }

        pdscp = xmalloc(sizeof *pdscp);
        pdscp->skb_priority = skb_priority;
        pdscp->dscp = (qdscp_list[i].dscp << 2) & IP_DSCP_MASK;
        hmap_insert(&xport->skb_priorities, &pdscp->hmap_node,
                    hash_int(pdscp->skb_priority, 0));
    }
}

static void
xlate_xport_remove(struct xlate_cfg *xcfg, struct xport *xport)
{
    if (!xport) {
        return;
    }

    if (xport->peer) {
        xport->peer->peer = NULL;
        xport->peer = NULL;
    }

    if (xport->xbundle) {
        list_remove(&xport->bundle_node);
    }

    clear_skb_priorities(xport);
    hmap_destroy(&xport->skb_priorities);

    hmap_remove(&xcfg->xports, &xport->hmap_node);
    hmap_remove(&xport->xbridge->xports, &xport->ofp_node);

    netdev_close(xport->netdev);
    cfm_unref(xport->cfm);
    bfd_unref(xport->bfd);
    free(xport);
}

void
xlate_ofport_remove(struct ofport_dpif *ofport)
{
    struct xport *xport;

    ovs_assert(new_xcfg);

    xport = xport_lookup(new_xcfg, ofport);
    xlate_xport_remove(new_xcfg, xport);
}

/* Given a datpath, packet, and flow metadata ('backer', 'packet', and 'key'
 * respectively), populates 'flow' with the result of odp_flow_key_to_flow().
 * Optionally populates 'ofproto' with the ofproto_dpif, 'odp_in_port' with
 * the datapath in_port, that 'packet' ingressed, and 'ipfix', 'sflow', and
 * 'netflow' with the appropriate handles for those protocols if they're
 * enabled.  Caller is responsible for unrefing them.
 *
 * If 'ofproto' is nonnull, requires 'flow''s in_port to exist.  Otherwise sets
 * 'flow''s in_port to OFPP_NONE.
 *
 * This function does post-processing on data returned from
 * odp_flow_key_to_flow() to help make VLAN splinters transparent to the rest
 * of the upcall processing logic.  In particular, if the extracted in_port is
 * a VLAN splinter port, it replaces flow->in_port by the "real" port, sets
 * flow->vlan_tci correctly for the VLAN of the VLAN splinter port, and pushes
 * a VLAN header onto 'packet' (if it is nonnull).
 *
 * Similarly, this function also includes some logic to help with tunnels.  It
 * may modify 'flow' as necessary to make the tunneling implementation
 * transparent to the upcall processing logic.
 *
 * Returns 0 if successful, ENODEV if the parsed flow has no associated ofport,
 * or some other positive errno if there are other problems. */
int
xlate_receive(const struct dpif_backer *backer, struct ofpbuf *packet,
              const struct nlattr *key, size_t key_len, struct flow *flow,
              struct ofproto_dpif **ofproto, struct dpif_ipfix **ipfix,
              struct dpif_sflow **sflow, struct netflow **netflow,
              odp_port_t *odp_in_port)
{
    struct xlate_cfg *xcfg = ovsrcu_get(struct xlate_cfg *, &xcfgp);
    int error = ENODEV;
    const struct xport *xport;

    if (odp_flow_key_to_flow(key, key_len, flow) == ODP_FIT_ERROR) {
        error = EINVAL;
        return error;
    }

    if (odp_in_port) {
        *odp_in_port = flow->in_port.odp_port;
    }

    xport = xport_lookup(xcfg, tnl_port_should_receive(flow)
                         ? tnl_port_receive(flow)
                         : odp_port_to_ofport(backer, flow->in_port.odp_port));

    flow->in_port.ofp_port = xport ? xport->ofp_port : OFPP_NONE;
    if (!xport) {
        return error;
    }

    if (vsp_adjust_flow(xport->xbridge->ofproto, flow)) {
        if (packet) {
            /* Make the packet resemble the flow, so that it gets sent to
             * an OpenFlow controller properly, so that it looks correct
             * for sFlow, and so that flow_extract() will get the correct
             * vlan_tci if it is called on 'packet'. */
            eth_push_vlan(packet, htons(ETH_TYPE_VLAN), flow->vlan_tci);
        }
    }
    error = 0;

    if (ofproto) {
        *ofproto = xport->xbridge->ofproto;
    }

    if (ipfix) {
        *ipfix = dpif_ipfix_ref(xport->xbridge->ipfix);
    }

    if (sflow) {
        *sflow = dpif_sflow_ref(xport->xbridge->sflow);
    }

    if (netflow) {
        *netflow = netflow_ref(xport->xbridge->netflow);
    }
    return error;
}

static struct xbridge *
xbridge_lookup(struct xlate_cfg *xcfg, const struct ofproto_dpif *ofproto)
{
    struct hmap *xbridges;
    struct xbridge *xbridge;

    if (!ofproto || !xcfg) {
        return NULL;
    }

    xbridges = &xcfg->xbridges;

    HMAP_FOR_EACH_IN_BUCKET (xbridge, hmap_node, hash_pointer(ofproto, 0),
                             xbridges) {
        if (xbridge->ofproto == ofproto) {
            return xbridge;
        }
    }
    return NULL;
}

static struct xbundle *
xbundle_lookup(struct xlate_cfg *xcfg, const struct ofbundle *ofbundle)
{
    struct hmap *xbundles;
    struct xbundle *xbundle;

    if (!ofbundle || !xcfg) {
        return NULL;
    }

    xbundles = &xcfg->xbundles;

    HMAP_FOR_EACH_IN_BUCKET (xbundle, hmap_node, hash_pointer(ofbundle, 0),
                             xbundles) {
        if (xbundle->ofbundle == ofbundle) {
            return xbundle;
        }
    }
    return NULL;
}

static struct xport *
xport_lookup(struct xlate_cfg *xcfg, const struct ofport_dpif *ofport)
{
    struct hmap *xports;
    struct xport *xport;

    if (!ofport || !xcfg) {
        return NULL;
    }

    xports = &xcfg->xports;

    HMAP_FOR_EACH_IN_BUCKET (xport, hmap_node, hash_pointer(ofport, 0),
                             xports) {
        if (xport->ofport == ofport) {
            return xport;
        }
    }
    return NULL;
}

static struct stp_port *
xport_get_stp_port(const struct xport *xport)
{
    return xport->xbridge->stp && xport->stp_port_no != -1
        ? stp_get_port(xport->xbridge->stp, xport->stp_port_no)
        : NULL;
}

static bool
xport_stp_learn_state(const struct xport *xport)
{
    struct stp_port *sp = xport_get_stp_port(xport);
    return stp_learn_in_state(sp ? stp_port_get_state(sp) : STP_DISABLED);
}

static bool
xport_stp_forward_state(const struct xport *xport)
{
    struct stp_port *sp = xport_get_stp_port(xport);
    return stp_forward_in_state(sp ? stp_port_get_state(sp) : STP_DISABLED);
}

static bool
xport_stp_listen_state(const struct xport *xport)
{
    struct stp_port *sp = xport_get_stp_port(xport);
    return stp_listen_in_state(sp ? stp_port_get_state(sp) : STP_DISABLED);
}

/* Returns true if STP should process 'flow'.  Sets fields in 'wc' that
 * were used to make the determination.*/
static bool
stp_should_process_flow(const struct flow *flow, struct flow_wildcards *wc)
{
    /* is_stp() also checks dl_type, but dl_type is always set in 'wc'. */
    memset(&wc->masks.dl_dst, 0xff, sizeof wc->masks.dl_dst);
    return is_stp(flow);
}

static void
stp_process_packet(const struct xport *xport, const struct ofpbuf *packet)
{
    struct stp_port *sp = xport_get_stp_port(xport);
    struct ofpbuf payload = *packet;
    struct eth_header *eth = ofpbuf_data(&payload);

    /* Sink packets on ports that have STP disabled when the bridge has
     * STP enabled. */
    if (!sp || stp_port_get_state(sp) == STP_DISABLED) {
        return;
    }

    /* Trim off padding on payload. */
    if (ofpbuf_size(&payload) > ntohs(eth->eth_type) + ETH_HEADER_LEN) {
        ofpbuf_set_size(&payload, ntohs(eth->eth_type) + ETH_HEADER_LEN);
    }

    if (ofpbuf_try_pull(&payload, ETH_HEADER_LEN + LLC_HEADER_LEN)) {
        stp_received_bpdu(sp, ofpbuf_data(&payload), ofpbuf_size(&payload));
    }
}

static struct xport *
get_ofp_port(const struct xbridge *xbridge, ofp_port_t ofp_port)
{
    struct xport *xport;

    HMAP_FOR_EACH_IN_BUCKET (xport, ofp_node, hash_ofp_port(ofp_port),
                             &xbridge->xports) {
        if (xport->ofp_port == ofp_port) {
            return xport;
        }
    }
    return NULL;
}

static odp_port_t
ofp_port_to_odp_port(const struct xbridge *xbridge, ofp_port_t ofp_port)
{
    const struct xport *xport = get_ofp_port(xbridge, ofp_port);
    return xport ? xport->odp_port : ODPP_NONE;
}

static bool
odp_port_is_alive(const struct xlate_ctx *ctx, ofp_port_t ofp_port)
{
    struct xport *xport;

    xport = get_ofp_port(ctx->xbridge, ofp_port);
    if (!xport || xport->config & OFPUTIL_PC_PORT_DOWN ||
        xport->state & OFPUTIL_PS_LINK_DOWN) {
        return false;
    }

    return true;
}

static struct ofputil_bucket *
group_first_live_bucket(const struct xlate_ctx *, const struct group_dpif *,
                        int depth);

static bool
group_is_alive(const struct xlate_ctx *ctx, uint32_t group_id, int depth)
{
    struct group_dpif *group;

    if (group_dpif_lookup(ctx->xbridge->ofproto, group_id, &group)) {
        struct ofputil_bucket *bucket;

        bucket = group_first_live_bucket(ctx, group, depth);
        group_dpif_unref(group);
        return bucket == NULL;
    }

    return false;
}

#define MAX_LIVENESS_RECURSION 128 /* Arbitrary limit */

static bool
bucket_is_alive(const struct xlate_ctx *ctx,
                struct ofputil_bucket *bucket, int depth)
{
    if (depth >= MAX_LIVENESS_RECURSION) {
        static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 1);

        VLOG_WARN_RL(&rl, "bucket chaining exceeded %d links",
                     MAX_LIVENESS_RECURSION);
        return false;
    }

    return (!ofputil_bucket_has_liveness(bucket)
            || (bucket->watch_port != OFPP_ANY
               && odp_port_is_alive(ctx, bucket->watch_port))
            || (bucket->watch_group != OFPG_ANY
               && group_is_alive(ctx, bucket->watch_group, depth + 1)));
}

static struct ofputil_bucket *
group_first_live_bucket(const struct xlate_ctx *ctx,
                        const struct group_dpif *group, int depth)
{
    struct ofputil_bucket *bucket;
    const struct list *buckets;

    group_dpif_get_buckets(group, &buckets);
    LIST_FOR_EACH (bucket, list_node, buckets) {
        if (bucket_is_alive(ctx, bucket, depth)) {
            return bucket;
        }
    }

    return NULL;
}

static struct ofputil_bucket *
group_best_live_bucket(const struct xlate_ctx *ctx,
                       const struct group_dpif *group,
                       uint32_t basis)
{
    struct ofputil_bucket *best_bucket = NULL;
    uint32_t best_score = 0;
    int i = 0;

    struct ofputil_bucket *bucket;
    const struct list *buckets;

    group_dpif_get_buckets(group, &buckets);
    LIST_FOR_EACH (bucket, list_node, buckets) {
        if (bucket_is_alive(ctx, bucket, 0)) {
            uint32_t score = (hash_int(i, basis) & 0xffff) * bucket->weight;
            if (score >= best_score) {
                best_bucket = bucket;
                best_score = score;
            }
        }
        i++;
    }

    return best_bucket;
}

static bool
xbundle_trunks_vlan(const struct xbundle *bundle, uint16_t vlan)
{
    return (bundle->vlan_mode != PORT_VLAN_ACCESS
            && (!bundle->trunks || bitmap_is_set(bundle->trunks, vlan)));
}

static bool
xbundle_includes_vlan(const struct xbundle *xbundle, uint16_t vlan)
{
    return vlan == xbundle->vlan || xbundle_trunks_vlan(xbundle, vlan);
}

static mirror_mask_t
xbundle_mirror_out(const struct xbridge *xbridge, struct xbundle *xbundle)
{
    return xbundle != &ofpp_none_bundle
        ? mirror_bundle_out(xbridge->mbridge, xbundle->ofbundle)
        : 0;
}

static mirror_mask_t
xbundle_mirror_src(const struct xbridge *xbridge, struct xbundle *xbundle)
{
    return xbundle != &ofpp_none_bundle
        ? mirror_bundle_src(xbridge->mbridge, xbundle->ofbundle)
        : 0;
}

static mirror_mask_t
xbundle_mirror_dst(const struct xbridge *xbridge, struct xbundle *xbundle)
{
    return xbundle != &ofpp_none_bundle
        ? mirror_bundle_dst(xbridge->mbridge, xbundle->ofbundle)
        : 0;
}

static struct xbundle *
lookup_input_bundle(const struct xbridge *xbridge, ofp_port_t in_port,
                    bool warn, struct xport **in_xportp)
{
    struct xport *xport;

    /* Find the port and bundle for the received packet. */
    xport = get_ofp_port(xbridge, in_port);
    if (in_xportp) {
        *in_xportp = xport;
    }
    if (xport && xport->xbundle) {
        return xport->xbundle;
    }

    /* Special-case OFPP_NONE (OF1.0) and OFPP_CONTROLLER (OF1.1+),
     * which a controller may use as the ingress port for traffic that
     * it is sourcing. */
    if (in_port == OFPP_CONTROLLER || in_port == OFPP_NONE) {
        return &ofpp_none_bundle;
    }

    /* Odd.  A few possible reasons here:
     *
     * - We deleted a port but there are still a few packets queued up
     *   from it.
     *
     * - Someone externally added a port (e.g. "ovs-dpctl add-if") that
     *   we don't know about.
     *
     * - The ofproto client didn't configure the port as part of a bundle.
     *   This is particularly likely to happen if a packet was received on the
     *   port after it was created, but before the client had a chance to
     *   configure its bundle.
     */
    if (warn) {
        static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 5);

        VLOG_WARN_RL(&rl, "bridge %s: received packet on unknown "
                     "port %"PRIu16, xbridge->name, in_port);
    }
    return NULL;
}

static void
add_mirror_actions(struct xlate_ctx *ctx, const struct flow *orig_flow)
{
    const struct xbridge *xbridge = ctx->xbridge;
    mirror_mask_t mirrors;
    struct xbundle *in_xbundle;
    uint16_t vlan;
    uint16_t vid;

    mirrors = ctx->xout->mirrors;
    ctx->xout->mirrors = 0;

    in_xbundle = lookup_input_bundle(xbridge, orig_flow->in_port.ofp_port,
                                     ctx->xin->packet != NULL, NULL);
    if (!in_xbundle) {
        return;
    }
    mirrors |= xbundle_mirror_src(xbridge, in_xbundle);

    /* Drop frames on bundles reserved for mirroring. */
    if (xbundle_mirror_out(xbridge, in_xbundle)) {
        if (ctx->xin->packet != NULL) {
            static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 5);
            VLOG_WARN_RL(&rl, "bridge %s: dropping packet received on port "
                         "%s, which is reserved exclusively for mirroring",
                         ctx->xbridge->name, in_xbundle->name);
        }
        ofpbuf_clear(&ctx->xout->odp_actions);
        return;
    }

    /* Check VLAN. */
    vid = vlan_tci_to_vid(orig_flow->vlan_tci);
    if (!input_vid_is_valid(vid, in_xbundle, ctx->xin->packet != NULL)) {
        return;
    }
    vlan = input_vid_to_vlan(in_xbundle, vid);

    if (!mirrors) {
        return;
    }

    /* Restore the original packet before adding the mirror actions. */
    ctx->xin->flow = *orig_flow;

    while (mirrors) {
        mirror_mask_t dup_mirrors;
        struct ofbundle *out;
        unsigned long *vlans;
        bool vlan_mirrored;
        bool has_mirror;
        int out_vlan;

        has_mirror = mirror_get(xbridge->mbridge, raw_ctz(mirrors),
                                &vlans, &dup_mirrors, &out, &out_vlan);
        ovs_assert(has_mirror);

        if (vlans) {
            ctx->xout->wc.masks.vlan_tci |= htons(VLAN_CFI | VLAN_VID_MASK);
        }
        vlan_mirrored = !vlans || bitmap_is_set(vlans, vlan);
        free(vlans);

        if (!vlan_mirrored) {
            mirrors = zero_rightmost_1bit(mirrors);
            continue;
        }

        mirrors &= ~dup_mirrors;
        ctx->xout->mirrors |= dup_mirrors;
        if (out) {
            struct xlate_cfg *xcfg = ovsrcu_get(struct xlate_cfg *, &xcfgp);
            struct xbundle *out_xbundle = xbundle_lookup(xcfg, out);
            if (out_xbundle) {
                output_normal(ctx, out_xbundle, vlan);
            }
        } else if (vlan != out_vlan
                   && !eth_addr_is_reserved(orig_flow->dl_dst)) {
            struct xbundle *xbundle;

            LIST_FOR_EACH (xbundle, list_node, &xbridge->xbundles) {
                if (xbundle_includes_vlan(xbundle, out_vlan)
                    && !xbundle_mirror_out(xbridge, xbundle)) {
                    output_normal(ctx, xbundle, out_vlan);
                }
            }
        }
    }
}

/* Given 'vid', the VID obtained from the 802.1Q header that was received as
 * part of a packet (specify 0 if there was no 802.1Q header), and 'in_xbundle',
 * the bundle on which the packet was received, returns the VLAN to which the
 * packet belongs.
 *
 * Both 'vid' and the return value are in the range 0...4095. */
static uint16_t
input_vid_to_vlan(const struct xbundle *in_xbundle, uint16_t vid)
{
    switch (in_xbundle->vlan_mode) {
    case PORT_VLAN_ACCESS:
        return in_xbundle->vlan;
        break;

    case PORT_VLAN_TRUNK:
        return vid;

    case PORT_VLAN_NATIVE_UNTAGGED:
    case PORT_VLAN_NATIVE_TAGGED:
        return vid ? vid : in_xbundle->vlan;

    default:
        OVS_NOT_REACHED();
    }
}

/* Checks whether a packet with the given 'vid' may ingress on 'in_xbundle'.
 * If so, returns true.  Otherwise, returns false and, if 'warn' is true, logs
 * a warning.
 *
 * 'vid' should be the VID obtained from the 802.1Q header that was received as
 * part of a packet (specify 0 if there was no 802.1Q header), in the range
 * 0...4095. */
static bool
input_vid_is_valid(uint16_t vid, struct xbundle *in_xbundle, bool warn)
{
    /* Allow any VID on the OFPP_NONE port. */
    if (in_xbundle == &ofpp_none_bundle) {
        return true;
    }

    switch (in_xbundle->vlan_mode) {
    case PORT_VLAN_ACCESS:
        if (vid) {
            if (warn) {
                static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 5);
                VLOG_WARN_RL(&rl, "dropping VLAN %"PRIu16" tagged "
                             "packet received on port %s configured as VLAN "
                             "%"PRIu16" access port", vid, in_xbundle->name,
                             in_xbundle->vlan);
            }
            return false;
        }
        return true;

    case PORT_VLAN_NATIVE_UNTAGGED:
    case PORT_VLAN_NATIVE_TAGGED:
        if (!vid) {
            /* Port must always carry its native VLAN. */
            return true;
        }
        /* Fall through. */
    case PORT_VLAN_TRUNK:
        if (!xbundle_includes_vlan(in_xbundle, vid)) {
            if (warn) {
                static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 5);
                VLOG_WARN_RL(&rl, "dropping VLAN %"PRIu16" packet "
                             "received on port %s not configured for trunking "
                             "VLAN %"PRIu16, vid, in_xbundle->name, vid);
            }
            return false;
        }
        return true;

    default:
        OVS_NOT_REACHED();
    }

}

/* Given 'vlan', the VLAN that a packet belongs to, and
 * 'out_xbundle', a bundle on which the packet is to be output, returns the VID
 * that should be included in the 802.1Q header.  (If the return value is 0,
 * then the 802.1Q header should only be included in the packet if there is a
 * nonzero PCP.)
 *
 * Both 'vlan' and the return value are in the range 0...4095. */
static uint16_t
output_vlan_to_vid(const struct xbundle *out_xbundle, uint16_t vlan)
{
    switch (out_xbundle->vlan_mode) {
    case PORT_VLAN_ACCESS:
        return 0;

    case PORT_VLAN_TRUNK:
    case PORT_VLAN_NATIVE_TAGGED:
        return vlan;

    case PORT_VLAN_NATIVE_UNTAGGED:
        return vlan == out_xbundle->vlan ? 0 : vlan;

    default:
        OVS_NOT_REACHED();
    }
}

static void
output_normal(struct xlate_ctx *ctx, const struct xbundle *out_xbundle,
              uint16_t vlan)
{
    ovs_be16 *flow_tci = &ctx->xin->flow.vlan_tci;
    uint16_t vid;
    ovs_be16 tci, old_tci;
    struct xport *xport;

    vid = output_vlan_to_vid(out_xbundle, vlan);
    if (list_is_empty(&out_xbundle->xports)) {
        /* Partially configured bundle with no slaves.  Drop the packet. */
        return;
    } else if (!out_xbundle->bond) {
        ctx->use_recirc = false;
        xport = CONTAINER_OF(list_front(&out_xbundle->xports), struct xport,
                             bundle_node);
    } else {
        struct xlate_cfg *xcfg = ovsrcu_get(struct xlate_cfg *, &xcfgp);
        struct flow_wildcards *wc = &ctx->xout->wc;
        struct xlate_recirc *xr = &ctx->recirc;
        struct ofport_dpif *ofport;

        if (ctx->xbridge->enable_recirc) {
            ctx->use_recirc = bond_may_recirc(
                out_xbundle->bond, &xr->recirc_id, &xr->hash_basis);

            if (ctx->use_recirc) {
                /* Only TCP mode uses recirculation. */
                xr->hash_alg = OVS_HASH_ALG_L4;
                bond_update_post_recirc_rules(out_xbundle->bond, false);

                /* Recirculation does not require unmasking hash fields. */
                wc = NULL;
            }
        }

        ofport = bond_choose_output_slave(out_xbundle->bond,
                                          &ctx->xin->flow, wc, vid);
        xport = xport_lookup(xcfg, ofport);

        if (!xport) {
            /* No slaves enabled, so drop packet. */
            return;
        }

        /* If ctx->xout->use_recirc is set, the main thread will handle stats
         * accounting for this bond. */
        if (!ctx->use_recirc) {
            if (ctx->xin->resubmit_stats) {
                bond_account(out_xbundle->bond, &ctx->xin->flow, vid,
                             ctx->xin->resubmit_stats->n_bytes);
            }
            if (ctx->xin->xcache) {
                struct xc_entry *entry;
                struct flow *flow;

                flow = &ctx->xin->flow;
                entry = xlate_cache_add_entry(ctx->xin->xcache, XC_BOND);
                entry->u.bond.bond = bond_ref(out_xbundle->bond);
                entry->u.bond.flow = xmemdup(flow, sizeof *flow);
                entry->u.bond.vid = vid;
            }
        }
    }

    old_tci = *flow_tci;
    tci = htons(vid);
    if (tci || out_xbundle->use_priority_tags) {
        tci |= *flow_tci & htons(VLAN_PCP_MASK);
        if (tci) {
            tci |= htons(VLAN_CFI);
        }
    }
    *flow_tci = tci;

    compose_output_action(ctx, xport->ofp_port);
    *flow_tci = old_tci;
}

/* A VM broadcasts a gratuitous ARP to indicate that it has resumed after
 * migration.  Older Citrix-patched Linux DomU used gratuitous ARP replies to
 * indicate this; newer upstream kernels use gratuitous ARP requests. */
static bool
is_gratuitous_arp(const struct flow *flow, struct flow_wildcards *wc)
{
    if (flow->dl_type != htons(ETH_TYPE_ARP)) {
        return false;
    }

    memset(&wc->masks.dl_dst, 0xff, sizeof wc->masks.dl_dst);
    if (!eth_addr_is_broadcast(flow->dl_dst)) {
        return false;
    }

    memset(&wc->masks.nw_proto, 0xff, sizeof wc->masks.nw_proto);
    if (flow->nw_proto == ARP_OP_REPLY) {
        return true;
    } else if (flow->nw_proto == ARP_OP_REQUEST) {
        memset(&wc->masks.nw_src, 0xff, sizeof wc->masks.nw_src);
        memset(&wc->masks.nw_dst, 0xff, sizeof wc->masks.nw_dst);

        return flow->nw_src == flow->nw_dst;
    } else {
        return false;
    }
}

/* Determines whether packets in 'flow' within 'xbridge' should be forwarded or
 * dropped.  Returns true if they may be forwarded, false if they should be
 * dropped.
 *
 * 'in_port' must be the xport that corresponds to flow->in_port.
 * 'in_port' must be part of a bundle (e.g. in_port->bundle must be nonnull).
 *
 * 'vlan' must be the VLAN that corresponds to flow->vlan_tci on 'in_port', as
 * returned by input_vid_to_vlan().  It must be a valid VLAN for 'in_port', as
 * checked by input_vid_is_valid().
 *
 * May also add tags to '*tags', although the current implementation only does
 * so in one special case.
 */
static bool
is_admissible(struct xlate_ctx *ctx, struct xport *in_port,
              uint16_t vlan)
{
    struct xbundle *in_xbundle = in_port->xbundle;
    const struct xbridge *xbridge = ctx->xbridge;
    struct flow *flow = &ctx->xin->flow;

    /* Drop frames for reserved multicast addresses
     * only if forward_bpdu option is absent. */
    if (!xbridge->forward_bpdu && eth_addr_is_reserved(flow->dl_dst)) {
        xlate_report(ctx, "packet has reserved destination MAC, dropping");
        return false;
    }

    if (in_xbundle->bond) {
        struct mac_entry *mac;

        switch (bond_check_admissibility(in_xbundle->bond, in_port->ofport,
                                         flow->dl_dst)) {
        case BV_ACCEPT:
            break;

        case BV_DROP:
            xlate_report(ctx, "bonding refused admissibility, dropping");
            return false;

        case BV_DROP_IF_MOVED:
            ovs_rwlock_rdlock(&xbridge->ml->rwlock);
            mac = mac_learning_lookup(xbridge->ml, flow->dl_src, vlan);
            if (mac && mac->port.p != in_xbundle->ofbundle &&
                (!is_gratuitous_arp(flow, &ctx->xout->wc)
                 || mac_entry_is_grat_arp_locked(mac))) {
                ovs_rwlock_unlock(&xbridge->ml->rwlock);
                xlate_report(ctx, "SLB bond thinks this packet looped back, "
                             "dropping");
                return false;
            }
            ovs_rwlock_unlock(&xbridge->ml->rwlock);
            break;
        }
    }

    return true;
}

/* Checks whether a MAC learning update is necessary for MAC learning table
 * 'ml' given that a packet matching 'flow' was received  on 'in_xbundle' in
 * 'vlan'.
 *
 * Most packets processed through the MAC learning table do not actually
 * change it in any way.  This function requires only a read lock on the MAC
 * learning table, so it is much cheaper in this common case.
 *
 * Keep the code here synchronized with that in update_learning_table__()
 * below. */
static bool
is_mac_learning_update_needed(const struct mac_learning *ml,
                              const struct flow *flow,
                              struct flow_wildcards *wc,
                              int vlan, struct xbundle *in_xbundle)
OVS_REQ_RDLOCK(ml->rwlock)
{
    struct mac_entry *mac;

    if (!mac_learning_may_learn(ml, flow->dl_src, vlan)) {
        return false;
    }

    mac = mac_learning_lookup(ml, flow->dl_src, vlan);
    if (!mac || mac_entry_age(ml, mac)) {
        return true;
    }

    if (is_gratuitous_arp(flow, wc)) {
        /* We don't want to learn from gratuitous ARP packets that are
         * reflected back over bond slaves so we lock the learning table. */
        if (!in_xbundle->bond) {
            return true;
        } else if (mac_entry_is_grat_arp_locked(mac)) {
            return false;
        }
    }

    return mac->port.p != in_xbundle->ofbundle;
}


/* Updates MAC learning table 'ml' given that a packet matching 'flow' was
 * received on 'in_xbundle' in 'vlan'.
 *
 * This code repeats all the checks in is_mac_learning_update_needed() because
 * the lock was released between there and here and thus the MAC learning state
 * could have changed.
 *
 * Keep the code here synchronized with that in is_mac_learning_update_needed()
 * above. */
static void
update_learning_table__(const struct xbridge *xbridge,
                        const struct flow *flow, struct flow_wildcards *wc,
                        int vlan, struct xbundle *in_xbundle)
OVS_REQ_WRLOCK(xbridge->ml->rwlock)
{
    struct mac_entry *mac;

    if (!mac_learning_may_learn(xbridge->ml, flow->dl_src, vlan)) {
        return;
    }

    mac = mac_learning_insert(xbridge->ml, flow->dl_src, vlan);
    if (is_gratuitous_arp(flow, wc)) {
        /* We don't want to learn from gratuitous ARP packets that are
         * reflected back over bond slaves so we lock the learning table. */
        if (!in_xbundle->bond) {
            mac_entry_set_grat_arp_lock(mac);
        } else if (mac_entry_is_grat_arp_locked(mac)) {
            return;
        }
    }

    if (mac->port.p != in_xbundle->ofbundle) {
        /* The log messages here could actually be useful in debugging,
         * so keep the rate limit relatively high. */
        static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(30, 300);

        VLOG_DBG_RL(&rl, "bridge %s: learned that "ETH_ADDR_FMT" is "
                    "on port %s in VLAN %d",
                    xbridge->name, ETH_ADDR_ARGS(flow->dl_src),
                    in_xbundle->name, vlan);

        mac->port.p = in_xbundle->ofbundle;
        mac_learning_changed(xbridge->ml);
    }
}

static void
update_learning_table(const struct xbridge *xbridge,
                      const struct flow *flow, struct flow_wildcards *wc,
                      int vlan, struct xbundle *in_xbundle)
{
    bool need_update;

    /* Don't learn the OFPP_NONE port. */
    if (in_xbundle == &ofpp_none_bundle) {
        return;
    }

    /* First try the common case: no change to MAC learning table. */
    ovs_rwlock_rdlock(&xbridge->ml->rwlock);
    need_update = is_mac_learning_update_needed(xbridge->ml, flow, wc, vlan,
                                                in_xbundle);
    ovs_rwlock_unlock(&xbridge->ml->rwlock);

    if (need_update) {
        /* Slow path: MAC learning table might need an update. */
        ovs_rwlock_wrlock(&xbridge->ml->rwlock);
        update_learning_table__(xbridge, flow, wc, vlan, in_xbundle);
        ovs_rwlock_unlock(&xbridge->ml->rwlock);
    }
}

static void
xlate_normal_flood(struct xlate_ctx *ctx, struct xbundle *in_xbundle,
                   uint16_t vlan)
{
    struct xbundle *xbundle;

    LIST_FOR_EACH (xbundle, list_node, &ctx->xbridge->xbundles) {
        if (xbundle != in_xbundle
            && xbundle_includes_vlan(xbundle, vlan)
            && xbundle->floodable
            && !xbundle_mirror_out(ctx->xbridge, xbundle)) {
            output_normal(ctx, xbundle, vlan);
        }
    }
    ctx->xout->nf_output_iface = NF_OUT_FLOOD;
}

static void
xlate_normal(struct xlate_ctx *ctx)
{
    struct flow_wildcards *wc = &ctx->xout->wc;
    struct flow *flow = &ctx->xin->flow;
    struct xbundle *in_xbundle;
    struct xport *in_port;
    struct mac_entry *mac;
    void *mac_port;
    uint16_t vlan;
    uint16_t vid;

    ctx->xout->has_normal = true;

    memset(&wc->masks.dl_src, 0xff, sizeof wc->masks.dl_src);
    memset(&wc->masks.dl_dst, 0xff, sizeof wc->masks.dl_dst);
    wc->masks.vlan_tci |= htons(VLAN_VID_MASK | VLAN_CFI);

    in_xbundle = lookup_input_bundle(ctx->xbridge, flow->in_port.ofp_port,
                                     ctx->xin->packet != NULL, &in_port);
    if (!in_xbundle) {
        xlate_report(ctx, "no input bundle, dropping");
        return;
    }

    /* Drop malformed frames. */
    if (flow->dl_type == htons(ETH_TYPE_VLAN) &&
        !(flow->vlan_tci & htons(VLAN_CFI))) {
        if (ctx->xin->packet != NULL) {
            static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 5);
            VLOG_WARN_RL(&rl, "bridge %s: dropping packet with partial "
                         "VLAN tag received on port %s",
                         ctx->xbridge->name, in_xbundle->name);
        }
        xlate_report(ctx, "partial VLAN tag, dropping");
        return;
    }

    /* Drop frames on bundles reserved for mirroring. */
    if (xbundle_mirror_out(ctx->xbridge, in_xbundle)) {
        if (ctx->xin->packet != NULL) {
            static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 5);
            VLOG_WARN_RL(&rl, "bridge %s: dropping packet received on port "
                         "%s, which is reserved exclusively for mirroring",
                         ctx->xbridge->name, in_xbundle->name);
        }
        xlate_report(ctx, "input port is mirror output port, dropping");
        return;
    }

    /* Check VLAN. */
    vid = vlan_tci_to_vid(flow->vlan_tci);
    if (!input_vid_is_valid(vid, in_xbundle, ctx->xin->packet != NULL)) {
        xlate_report(ctx, "disallowed VLAN VID for this input port, dropping");
        return;
    }
    vlan = input_vid_to_vlan(in_xbundle, vid);

    /* Check other admissibility requirements. */
    if (in_port && !is_admissible(ctx, in_port, vlan)) {
        return;
    }

    /* Learn source MAC. */
    if (ctx->xin->may_learn) {
        update_learning_table(ctx->xbridge, flow, wc, vlan, in_xbundle);
    }
    if (ctx->xin->xcache) {
        struct xc_entry *entry;

        /* Save enough info to update mac learning table later. */
        entry = xlate_cache_add_entry(ctx->xin->xcache, XC_NORMAL);
        entry->u.normal.ofproto = ctx->xbridge->ofproto;
        entry->u.normal.flow = xmemdup(flow, sizeof *flow);
        entry->u.normal.vlan = vlan;
    }

    /* Determine output bundle. */
    ovs_rwlock_rdlock(&ctx->xbridge->ml->rwlock);
    mac = mac_learning_lookup(ctx->xbridge->ml, flow->dl_dst, vlan);
    mac_port = mac ? mac->port.p : NULL;
    ovs_rwlock_unlock(&ctx->xbridge->ml->rwlock);

    if (mac_port) {
        struct xlate_cfg *xcfg = ovsrcu_get(struct xlate_cfg *, &xcfgp);
        struct xbundle *mac_xbundle = xbundle_lookup(xcfg, mac_port);
        if (mac_xbundle && mac_xbundle != in_xbundle) {
            xlate_report(ctx, "forwarding to learned port");
            output_normal(ctx, mac_xbundle, vlan);
        } else if (!mac_xbundle) {
            xlate_report(ctx, "learned port is unknown, dropping");
        } else {
            xlate_report(ctx, "learned port is input port, dropping");
        }
    } else {
        xlate_report(ctx, "no learned MAC for destination, flooding");
        xlate_normal_flood(ctx, in_xbundle, vlan);
    }
}

/* Compose SAMPLE action for sFlow or IPFIX.  The given probability is
 * the number of packets out of UINT32_MAX to sample.  The given
 * cookie is passed back in the callback for each sampled packet.
 */
static size_t
compose_sample_action(const struct xbridge *xbridge,
                      struct ofpbuf *odp_actions,
                      const struct flow *flow,
                      const uint32_t probability,
                      const union user_action_cookie *cookie,
                      const size_t cookie_size)
{
    size_t sample_offset, actions_offset;
    odp_port_t odp_port;
    int cookie_offset;
    uint32_t pid;

    sample_offset = nl_msg_start_nested(odp_actions, OVS_ACTION_ATTR_SAMPLE);

    nl_msg_put_u32(odp_actions, OVS_SAMPLE_ATTR_PROBABILITY, probability);

    actions_offset = nl_msg_start_nested(odp_actions, OVS_SAMPLE_ATTR_ACTIONS);

    odp_port = ofp_port_to_odp_port(xbridge, flow->in_port.ofp_port);
    pid = dpif_port_get_pid(xbridge->dpif, odp_port,
                            flow_hash_5tuple(flow, 0));
    cookie_offset = odp_put_userspace_action(pid, cookie, cookie_size,
                                             odp_actions);

    nl_msg_end_nested(odp_actions, actions_offset);
    nl_msg_end_nested(odp_actions, sample_offset);
    return cookie_offset;
}

static void
compose_sflow_cookie(const struct xbridge *xbridge, ovs_be16 vlan_tci,
                     odp_port_t odp_port, unsigned int n_outputs,
                     union user_action_cookie *cookie)
{
    int ifindex;

    cookie->type = USER_ACTION_COOKIE_SFLOW;
    cookie->sflow.vlan_tci = vlan_tci;

    /* See http://www.sflow.org/sflow_version_5.txt (search for "Input/output
     * port information") for the interpretation of cookie->output. */
    switch (n_outputs) {
    case 0:
        /* 0x40000000 | 256 means "packet dropped for unknown reason". */
        cookie->sflow.output = 0x40000000 | 256;
        break;

    case 1:
        ifindex = dpif_sflow_odp_port_to_ifindex(xbridge->sflow, odp_port);
        if (ifindex) {
            cookie->sflow.output = ifindex;
            break;
        }
        /* Fall through. */
    default:
        /* 0x80000000 means "multiple output ports. */
        cookie->sflow.output = 0x80000000 | n_outputs;
        break;
    }
}

/* Compose SAMPLE action for sFlow bridge sampling. */
static size_t
compose_sflow_action(const struct xbridge *xbridge,
                     struct ofpbuf *odp_actions,
                     const struct flow *flow,
                     odp_port_t odp_port)
{
    uint32_t probability;
    union user_action_cookie cookie;

    if (!xbridge->sflow || flow->in_port.ofp_port == OFPP_NONE) {
        return 0;
    }

    probability = dpif_sflow_get_probability(xbridge->sflow);
    compose_sflow_cookie(xbridge, htons(0), odp_port,
                         odp_port == ODPP_NONE ? 0 : 1, &cookie);

    return compose_sample_action(xbridge, odp_actions, flow,  probability,
                                 &cookie, sizeof cookie.sflow);
}

static void
compose_flow_sample_cookie(uint16_t probability, uint32_t collector_set_id,
                           uint32_t obs_domain_id, uint32_t obs_point_id,
                           union user_action_cookie *cookie)
{
    cookie->type = USER_ACTION_COOKIE_FLOW_SAMPLE;
    cookie->flow_sample.probability = probability;
    cookie->flow_sample.collector_set_id = collector_set_id;
    cookie->flow_sample.obs_domain_id = obs_domain_id;
    cookie->flow_sample.obs_point_id = obs_point_id;
}

static void
compose_ipfix_cookie(union user_action_cookie *cookie)
{
    cookie->type = USER_ACTION_COOKIE_IPFIX;
}

/* Compose SAMPLE action for IPFIX bridge sampling. */
static void
compose_ipfix_action(const struct xbridge *xbridge,
                     struct ofpbuf *odp_actions,
                     const struct flow *flow)
{
    uint32_t probability;
    union user_action_cookie cookie;

    if (!xbridge->ipfix || flow->in_port.ofp_port == OFPP_NONE) {
        return;
    }

    probability = dpif_ipfix_get_bridge_exporter_probability(xbridge->ipfix);
    compose_ipfix_cookie(&cookie);

    compose_sample_action(xbridge, odp_actions, flow,  probability,
                          &cookie, sizeof cookie.ipfix);
}

/* SAMPLE action for sFlow must be first action in any given list of
 * actions.  At this point we do not have all information required to
 * build it. So try to build sample action as complete as possible. */
static void
add_sflow_action(struct xlate_ctx *ctx)
{
    ctx->user_cookie_offset = compose_sflow_action(ctx->xbridge,
                                                   &ctx->xout->odp_actions,
                                                   &ctx->xin->flow, ODPP_NONE);
    ctx->sflow_odp_port = 0;
    ctx->sflow_n_outputs = 0;
}

/* SAMPLE action for IPFIX must be 1st or 2nd action in any given list
 * of actions, eventually after the SAMPLE action for sFlow. */
static void
add_ipfix_action(struct xlate_ctx *ctx)
{
    compose_ipfix_action(ctx->xbridge, &ctx->xout->odp_actions,
                         &ctx->xin->flow);
}

/* Fix SAMPLE action according to data collected while composing ODP actions.
 * We need to fix SAMPLE actions OVS_SAMPLE_ATTR_ACTIONS attribute, i.e. nested
 * USERSPACE action's user-cookie which is required for sflow. */
static void
fix_sflow_action(struct xlate_ctx *ctx)
{
    const struct flow *base = &ctx->base_flow;
    union user_action_cookie *cookie;

    if (!ctx->user_cookie_offset) {
        return;
    }

    cookie = ofpbuf_at(&ctx->xout->odp_actions, ctx->user_cookie_offset,
                       sizeof cookie->sflow);
    ovs_assert(cookie->type == USER_ACTION_COOKIE_SFLOW);

    compose_sflow_cookie(ctx->xbridge, base->vlan_tci,
                         ctx->sflow_odp_port, ctx->sflow_n_outputs, cookie);
}

static enum slow_path_reason
process_special(struct xlate_ctx *ctx, const struct flow *flow,
                const struct xport *xport, const struct ofpbuf *packet)
{
    struct flow_wildcards *wc = &ctx->xout->wc;
    const struct xbridge *xbridge = ctx->xbridge;

    if (!xport) {
        return 0;
    } else if (xport->cfm && cfm_should_process_flow(xport->cfm, flow, wc)) {
        if (packet) {
            cfm_process_heartbeat(xport->cfm, packet);
        }
        return SLOW_CFM;
    } else if (xport->bfd && bfd_should_process_flow(xport->bfd, flow, wc)) {
        if (packet) {
            bfd_process_packet(xport->bfd, flow, packet);
            /* If POLL received, immediately sends FINAL back. */
            if (bfd_should_send_packet(xport->bfd)) {
                ofproto_dpif_monitor_port_send_soon(xport->ofport);
            }
        }
        return SLOW_BFD;
    } else if (xport->xbundle && xport->xbundle->lacp
               && flow->dl_type == htons(ETH_TYPE_LACP)) {
        if (packet) {
            lacp_process_packet(xport->xbundle->lacp, xport->ofport, packet);
        }
        return SLOW_LACP;
    } else if (xbridge->stp && stp_should_process_flow(flow, wc)) {
        if (packet) {
            stp_process_packet(xport, packet);
        }
        return SLOW_STP;
    } else {
        return 0;
    }
}

static void
compose_output_action__(struct xlate_ctx *ctx, ofp_port_t ofp_port,
                        bool check_stp)
{
    const struct xport *xport = get_ofp_port(ctx->xbridge, ofp_port);
    struct flow_wildcards *wc = &ctx->xout->wc;
    struct flow *flow = &ctx->xin->flow;
    ovs_be16 flow_vlan_tci;
    uint32_t flow_pkt_mark;
    uint8_t flow_nw_tos;
    odp_port_t out_port, odp_port;
    uint8_t dscp;

    /* If 'struct flow' gets additional metadata, we'll need to zero it out
     * before traversing a patch port. */
    BUILD_ASSERT_DECL(FLOW_WC_SEQ == 26);

    if (!xport) {
        xlate_report(ctx, "Nonexistent output port");
        return;
    } else if (xport->config & OFPUTIL_PC_NO_FWD) {
        xlate_report(ctx, "OFPPC_NO_FWD set, skipping output");
        return;
    } else if (check_stp) {
        if (is_stp(&ctx->base_flow)) {
            if (!xport_stp_listen_state(xport)) {
                xlate_report(ctx, "STP not in listening state, "
                             "skipping bpdu output");
                return;
            }
        } else if (!xport_stp_forward_state(xport)) {
            xlate_report(ctx, "STP not in forwarding state, "
                         "skipping output");
            return;
        }
    }

    if (mbridge_has_mirrors(ctx->xbridge->mbridge) && xport->xbundle) {
        ctx->xout->mirrors |= xbundle_mirror_dst(xport->xbundle->xbridge,
                                                 xport->xbundle);
    }

    if (xport->peer) {
        const struct xport *peer = xport->peer;
        struct flow old_flow = ctx->xin->flow;
        enum slow_path_reason special;

        ctx->xbridge = peer->xbridge;
        flow->in_port.ofp_port = peer->ofp_port;
        flow->metadata = htonll(0);
        memset(&flow->tunnel, 0, sizeof flow->tunnel);
        memset(flow->regs, 0, sizeof flow->regs);

        special = process_special(ctx, &ctx->xin->flow, peer,
                                  ctx->xin->packet);
        if (special) {
            ctx->xout->slow |= special;
        } else if (may_receive(peer, ctx)) {
            if (xport_stp_forward_state(peer)) {
                xlate_table_action(ctx, flow->in_port.ofp_port, 0, true, true);
            } else {
                /* Forwarding is disabled by STP.  Let OFPP_NORMAL and the
                 * learning action look at the packet, then drop it. */
                struct flow old_base_flow = ctx->base_flow;
                size_t old_size = ofpbuf_size(&ctx->xout->odp_actions);
                mirror_mask_t old_mirrors = ctx->xout->mirrors;
                xlate_table_action(ctx, flow->in_port.ofp_port, 0, true, true);
                ctx->xout->mirrors = old_mirrors;
                ctx->base_flow = old_base_flow;
                ofpbuf_set_size(&ctx->xout->odp_actions, old_size);
            }
        }

        ctx->xin->flow = old_flow;
        ctx->xbridge = xport->xbridge;

        if (ctx->xin->resubmit_stats) {
            netdev_vport_inc_tx(xport->netdev, ctx->xin->resubmit_stats);
            netdev_vport_inc_rx(peer->netdev, ctx->xin->resubmit_stats);
            if (peer->bfd) {
                bfd_account_rx(peer->bfd, ctx->xin->resubmit_stats);
            }
        }
        if (ctx->xin->xcache) {
            struct xc_entry *entry;

            entry = xlate_cache_add_entry(ctx->xin->xcache, XC_NETDEV);
            entry->u.dev.tx = netdev_ref(xport->netdev);
            entry->u.dev.rx = netdev_ref(peer->netdev);
            entry->u.dev.bfd = bfd_ref(peer->bfd);
        }
        return;
    }

    flow_vlan_tci = flow->vlan_tci;
    flow_pkt_mark = flow->pkt_mark;
    flow_nw_tos = flow->nw_tos;

    if (dscp_from_skb_priority(xport, flow->skb_priority, &dscp)) {
        wc->masks.nw_tos |= IP_DSCP_MASK;
        flow->nw_tos &= ~IP_DSCP_MASK;
        flow->nw_tos |= dscp;
    }

    if (xport->is_tunnel) {
         /* Save tunnel metadata so that changes made due to
          * the Logical (tunnel) Port are not visible for any further
          * matches, while explicit set actions on tunnel metadata are.
          */
        struct flow_tnl flow_tnl = flow->tunnel;
        odp_port = tnl_port_send(xport->ofport, flow, &ctx->xout->wc);
        if (odp_port == ODPP_NONE) {
            xlate_report(ctx, "Tunneling decided against output");
            goto out; /* restore flow_nw_tos */
        }
        if (flow->tunnel.ip_dst == ctx->orig_tunnel_ip_dst) {
            xlate_report(ctx, "Not tunneling to our own address");
            goto out; /* restore flow_nw_tos */
        }
        if (ctx->xin->resubmit_stats) {
            netdev_vport_inc_tx(xport->netdev, ctx->xin->resubmit_stats);
        }
        if (ctx->xin->xcache) {
            struct xc_entry *entry;

            entry = xlate_cache_add_entry(ctx->xin->xcache, XC_NETDEV);
            entry->u.dev.tx = netdev_ref(xport->netdev);
        }
        out_port = odp_port;
        commit_odp_tunnel_action(flow, &ctx->base_flow,
                                 &ctx->xout->odp_actions);
        flow->tunnel = flow_tnl; /* Restore tunnel metadata */
    } else {
        odp_port = xport->odp_port;
        out_port = odp_port;
        if (ofproto_has_vlan_splinters(ctx->xbridge->ofproto)) {
            ofp_port_t vlandev_port;

            wc->masks.vlan_tci |= htons(VLAN_VID_MASK | VLAN_CFI);
            vlandev_port = vsp_realdev_to_vlandev(ctx->xbridge->ofproto,
                                                  ofp_port, flow->vlan_tci);
            if (vlandev_port != ofp_port) {
                out_port = ofp_port_to_odp_port(ctx->xbridge, vlandev_port);
                flow->vlan_tci = htons(0);
            }
        }
    }

    if (out_port != ODPP_NONE) {
        ctx->xout->slow |= commit_odp_actions(flow, &ctx->base_flow,
                                              &ctx->xout->odp_actions,
                                              &ctx->xout->wc);

        if (ctx->use_recirc) {
            struct ovs_action_hash *act_hash;
            struct xlate_recirc *xr = &ctx->recirc;

            /* Hash action. */
            act_hash = nl_msg_put_unspec_uninit(&ctx->xout->odp_actions,
                                                OVS_ACTION_ATTR_HASH,
                                                sizeof *act_hash);
            act_hash->hash_alg = xr->hash_alg;
            act_hash->hash_basis = xr->hash_basis;

            /* Recirc action. */
            nl_msg_put_u32(&ctx->xout->odp_actions, OVS_ACTION_ATTR_RECIRC,
                           xr->recirc_id);
        } else {
            nl_msg_put_odp_port(&ctx->xout->odp_actions, OVS_ACTION_ATTR_OUTPUT,
                                out_port);
        }

        ctx->sflow_odp_port = odp_port;
        ctx->sflow_n_outputs++;
        ctx->xout->nf_output_iface = ofp_port;
    }

 out:
    /* Restore flow */
    flow->vlan_tci = flow_vlan_tci;
    flow->pkt_mark = flow_pkt_mark;
    flow->nw_tos = flow_nw_tos;
}

static void
compose_output_action(struct xlate_ctx *ctx, ofp_port_t ofp_port)
{
    compose_output_action__(ctx, ofp_port, true);
}

static void
xlate_recursively(struct xlate_ctx *ctx, struct rule_dpif *rule)
{
    struct rule_dpif *old_rule = ctx->rule;
    const struct rule_actions *actions;

    if (ctx->xin->resubmit_stats) {
        rule_dpif_credit_stats(rule, ctx->xin->resubmit_stats);
    }

    ctx->resubmits++;
    ctx->recurse++;
    ctx->rule = rule;
    actions = rule_dpif_get_actions(rule);
    do_xlate_actions(actions->ofpacts, actions->ofpacts_len, ctx);
    ctx->rule = old_rule;
    ctx->recurse--;
}

static bool
xlate_resubmit_resource_check(struct xlate_ctx *ctx)
{
    static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 1);

    if (ctx->recurse >= MAX_RESUBMIT_RECURSION + MAX_INTERNAL_RESUBMITS) {
        VLOG_ERR_RL(&rl, "resubmit actions recursed over %d times",
                    MAX_RESUBMIT_RECURSION);
    } else if (ctx->resubmits >= MAX_RESUBMITS + MAX_INTERNAL_RESUBMITS) {
        VLOG_ERR_RL(&rl, "over %d resubmit actions", MAX_RESUBMITS);
    } else if (ofpbuf_size(&ctx->xout->odp_actions) > UINT16_MAX) {
        VLOG_ERR_RL(&rl, "resubmits yielded over 64 kB of actions");
    } else if (ofpbuf_size(&ctx->stack) >= 65536) {
        VLOG_ERR_RL(&rl, "resubmits yielded over 64 kB of stack");
    } else {
        return true;
    }

    return false;
}

static void
xlate_table_action(struct xlate_ctx *ctx, ofp_port_t in_port, uint8_t table_id,
                   bool may_packet_in, bool honor_table_miss)
{
    if (xlate_resubmit_resource_check(ctx)) {
        ofp_port_t old_in_port = ctx->xin->flow.in_port.ofp_port;
        bool skip_wildcards = ctx->xin->skip_wildcards;
        uint8_t old_table_id = ctx->table_id;
        struct rule_dpif *rule;
        enum rule_dpif_lookup_verdict verdict;
        enum ofputil_port_config config = 0;

        ctx->table_id = table_id;

        /* Look up a flow with 'in_port' as the input port.  Then restore the
         * original input port (otherwise OFPP_NORMAL and OFPP_IN_PORT will
         * have surprising behavior). */
        ctx->xin->flow.in_port.ofp_port = in_port;
        verdict = rule_dpif_lookup_from_table(ctx->xbridge->ofproto,
                                              &ctx->xin->flow,
                                              !skip_wildcards
                                              ? &ctx->xout->wc : NULL,
                                              honor_table_miss,
                                              &ctx->table_id, &rule,
                                              ctx->xin->xcache != NULL,
                                              ctx->xin->resubmit_stats);
        ctx->xin->flow.in_port.ofp_port = old_in_port;

        if (ctx->xin->resubmit_hook) {
            ctx->xin->resubmit_hook(ctx->xin, rule, ctx->recurse);
        }

        switch (verdict) {
        case RULE_DPIF_LOOKUP_VERDICT_MATCH:
           goto match;
        case RULE_DPIF_LOOKUP_VERDICT_CONTROLLER:
            if (may_packet_in) {
                struct xport *xport;

                xport = get_ofp_port(ctx->xbridge,
                                     ctx->xin->flow.in_port.ofp_port);
                config = xport ? xport->config : 0;
                break;
            }
            /* Fall through to drop */
        case RULE_DPIF_LOOKUP_VERDICT_DROP:
            config = OFPUTIL_PC_NO_PACKET_IN;
            break;
        case RULE_DPIF_LOOKUP_VERDICT_DEFAULT:
            if (!ofproto_dpif_wants_packet_in_on_miss(ctx->xbridge->ofproto)) {
                config = OFPUTIL_PC_NO_PACKET_IN;
            }
            break;
        default:
            OVS_NOT_REACHED();
        }

        choose_miss_rule(config, ctx->xbridge->miss_rule,
                         ctx->xbridge->no_packet_in_rule, &rule,
                         ctx->xin->xcache != NULL);

match:
        if (rule) {
            /* Fill in the cache entry here instead of xlate_recursively
             * to make the reference counting more explicit.  We take a
             * reference in the lookups above if we are going to cache the
             * rule. */
            if (ctx->xin->xcache) {
                struct xc_entry *entry;

                entry = xlate_cache_add_entry(ctx->xin->xcache, XC_RULE);
                entry->u.rule = rule;
            }
            xlate_recursively(ctx, rule);
        }

        ctx->table_id = old_table_id;
        return;
    }

    ctx->exit = true;
}

static void
xlate_group_stats(struct xlate_ctx *ctx, struct group_dpif *group,
                  struct ofputil_bucket *bucket)
{
    if (ctx->xin->resubmit_stats) {
        group_dpif_credit_stats(group, bucket, ctx->xin->resubmit_stats);
    }
    if (ctx->xin->xcache) {
        struct xc_entry *entry;

        entry = xlate_cache_add_entry(ctx->xin->xcache, XC_GROUP);
        entry->u.group.group = group_dpif_ref(group);
        entry->u.group.bucket = bucket;
    }
}

static void
xlate_group_bucket(struct xlate_ctx *ctx, struct ofputil_bucket *bucket)
{
    uint64_t action_list_stub[1024 / 8];
    struct ofpbuf action_list, action_set;

    ofpbuf_use_const(&action_set, bucket->ofpacts, bucket->ofpacts_len);
    ofpbuf_use_stub(&action_list, action_list_stub, sizeof action_list_stub);

    ofpacts_execute_action_set(&action_list, &action_set);
    ctx->recurse++;
    do_xlate_actions(ofpbuf_data(&action_list), ofpbuf_size(&action_list), ctx);
    ctx->recurse--;

    ofpbuf_uninit(&action_set);
    ofpbuf_uninit(&action_list);
}

static void
xlate_all_group(struct xlate_ctx *ctx, struct group_dpif *group)
{
    struct ofputil_bucket *bucket;
    const struct list *buckets;
    struct flow old_flow = ctx->xin->flow;

    group_dpif_get_buckets(group, &buckets);

    LIST_FOR_EACH (bucket, list_node, buckets) {
        xlate_group_bucket(ctx, bucket);
        /* Roll back flow to previous state.
         * This is equivalent to cloning the packet for each bucket.
         *
         * As a side effect any subsequently applied actions will
         * also effectively be applied to a clone of the packet taken
         * just before applying the all or indirect group. */
        ctx->xin->flow = old_flow;
    }
    xlate_group_stats(ctx, group, NULL);
}

static void
xlate_ff_group(struct xlate_ctx *ctx, struct group_dpif *group)
{
    struct ofputil_bucket *bucket;

    bucket = group_first_live_bucket(ctx, group, 0);
    if (bucket) {
        xlate_group_bucket(ctx, bucket);
        xlate_group_stats(ctx, group, bucket);
    }
}

static void
xlate_select_group(struct xlate_ctx *ctx, struct group_dpif *group)
{
    struct flow_wildcards *wc = &ctx->xout->wc;
    struct ofputil_bucket *bucket;
    uint32_t basis;

    basis = hash_mac(ctx->xin->flow.dl_dst, 0, 0);
    bucket = group_best_live_bucket(ctx, group, basis);
    if (bucket) {
        memset(&wc->masks.dl_dst, 0xff, sizeof wc->masks.dl_dst);
        xlate_group_bucket(ctx, bucket);
        xlate_group_stats(ctx, group, bucket);
    }
}

static void
xlate_group_action__(struct xlate_ctx *ctx, struct group_dpif *group)
{
    ctx->in_group = true;

    switch (group_dpif_get_type(group)) {
    case OFPGT11_ALL:
    case OFPGT11_INDIRECT:
        xlate_all_group(ctx, group);
        break;
    case OFPGT11_SELECT:
        xlate_select_group(ctx, group);
        break;
    case OFPGT11_FF:
        xlate_ff_group(ctx, group);
        break;
    default:
        OVS_NOT_REACHED();
    }
    group_dpif_unref(group);

    ctx->in_group = false;
}

static bool
xlate_group_resource_check(struct xlate_ctx *ctx)
{
    if (!xlate_resubmit_resource_check(ctx)) {
        return false;
    } else if (ctx->in_group) {
        /* Prevent nested translation of OpenFlow groups.
         *
         * OpenFlow allows this restriction.  We enforce this restriction only
         * because, with the current architecture, we would otherwise have to
         * take a possibly recursive read lock on the ofgroup rwlock, which is
         * unsafe given that POSIX allows taking a read lock to block if there
         * is a thread blocked on taking the write lock.  Other solutions
         * without this restriction are also possible, but seem unwarranted
         * given the current limited use of groups. */
        static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 1);

        VLOG_ERR_RL(&rl, "cannot recursively translate OpenFlow group");
        return false;
    } else {
        return true;
    }
}

static bool
xlate_group_action(struct xlate_ctx *ctx, uint32_t group_id)
{
    if (xlate_group_resource_check(ctx)) {
        struct group_dpif *group;
        bool got_group;

        got_group = group_dpif_lookup(ctx->xbridge->ofproto, group_id, &group);
        if (got_group) {
            xlate_group_action__(ctx, group);
        } else {
            return true;
        }
    }

    return false;
}

static void
xlate_ofpact_resubmit(struct xlate_ctx *ctx,
                      const struct ofpact_resubmit *resubmit)
{
    ofp_port_t in_port;
    uint8_t table_id;
    bool may_packet_in = false;
    bool honor_table_miss = false;

    if (ctx->rule && rule_dpif_is_internal(ctx->rule)) {
        /* Still allow missed packets to be sent to the controller
         * if resubmitting from an internal table. */
        may_packet_in = true;
        honor_table_miss = true;
    }

    in_port = resubmit->in_port;
    if (in_port == OFPP_IN_PORT) {
        in_port = ctx->xin->flow.in_port.ofp_port;
    }

    table_id = resubmit->table_id;
    if (table_id == 255) {
        table_id = ctx->table_id;
    }

    xlate_table_action(ctx, in_port, table_id, may_packet_in,
                       honor_table_miss);
}

static void
flood_packets(struct xlate_ctx *ctx, bool all)
{
    const struct xport *xport;

    HMAP_FOR_EACH (xport, ofp_node, &ctx->xbridge->xports) {
        if (xport->ofp_port == ctx->xin->flow.in_port.ofp_port) {
            continue;
        }

        if (all) {
            compose_output_action__(ctx, xport->ofp_port, false);
        } else if (!(xport->config & OFPUTIL_PC_NO_FLOOD)) {
            compose_output_action(ctx, xport->ofp_port);
        }
    }

    ctx->xout->nf_output_iface = NF_OUT_FLOOD;
}

static void
execute_controller_action(struct xlate_ctx *ctx, int len,
                          enum ofp_packet_in_reason reason,
                          uint16_t controller_id)
{
    struct ofproto_packet_in *pin;
    struct ofpbuf *packet;
    struct pkt_metadata md = PKT_METADATA_INITIALIZER(0);

    ctx->xout->slow |= SLOW_CONTROLLER;
    if (!ctx->xin->packet) {
        return;
    }

    packet = ofpbuf_clone(ctx->xin->packet);

    ctx->xout->slow |= commit_odp_actions(&ctx->xin->flow, &ctx->base_flow,
                                          &ctx->xout->odp_actions,
                                          &ctx->xout->wc);

    odp_execute_actions(NULL, packet, false, &md,
                        ofpbuf_data(&ctx->xout->odp_actions),
                        ofpbuf_size(&ctx->xout->odp_actions), NULL);

    pin = xmalloc(sizeof *pin);
    pin->up.packet_len = ofpbuf_size(packet);
    pin->up.packet = ofpbuf_steal_data(packet);
    pin->up.reason = reason;
    pin->up.table_id = ctx->table_id;
    pin->up.cookie = (ctx->rule
                      ? rule_dpif_get_flow_cookie(ctx->rule)
                      : OVS_BE64_MAX);

    flow_get_metadata(&ctx->xin->flow, &pin->up.fmd);

    pin->controller_id = controller_id;
    pin->send_len = len;
    /* If a rule is a table-miss rule then this is
     * a table-miss handled by a table-miss rule.
     *
     * Else, if rule is internal and has a controller action,
     * the later being implied by the rule being processed here,
     * then this is a table-miss handled without a table-miss rule.
     *
     * Otherwise this is not a table-miss. */
    pin->miss_type = OFPROTO_PACKET_IN_NO_MISS;
    if (ctx->rule) {
        if (rule_dpif_is_table_miss(ctx->rule)) {
            pin->miss_type = OFPROTO_PACKET_IN_MISS_FLOW;
        } else if (rule_dpif_is_internal(ctx->rule)) {
            pin->miss_type = OFPROTO_PACKET_IN_MISS_WITHOUT_FLOW;
        }
    }
    ofproto_dpif_send_packet_in(ctx->xbridge->ofproto, pin);
    ofpbuf_delete(packet);
}

static void
compose_mpls_push_action(struct xlate_ctx *ctx, struct ofpact_push_mpls *mpls)
{
    struct flow_wildcards *wc = &ctx->xout->wc;
    struct flow *flow = &ctx->xin->flow;
    int n;

    ovs_assert(eth_type_mpls(mpls->ethertype));

    n = flow_count_mpls_labels(flow, wc);
    if (!n) {
        ctx->xout->slow |= commit_odp_actions(flow, &ctx->base_flow,
                                              &ctx->xout->odp_actions,
                                              &ctx->xout->wc);
    } else if (n >= FLOW_MAX_MPLS_LABELS) {
        if (ctx->xin->packet != NULL) {
            static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 5);
            VLOG_WARN_RL(&rl, "bridge %s: dropping packet on which an "
                         "MPLS push action can't be performed as it would "
                         "have more MPLS LSEs than the %d supported.",
                         ctx->xbridge->name, FLOW_MAX_MPLS_LABELS);
        }
        ctx->exit = true;
        return;
    } else if (n >= ctx->xbridge->max_mpls_depth) {
        COVERAGE_INC(xlate_actions_mpls_overflow);
        ctx->xout->slow |= SLOW_ACTION;
    }

    flow_push_mpls(flow, n, mpls->ethertype, wc);
}

static void
compose_mpls_pop_action(struct xlate_ctx *ctx, ovs_be16 eth_type)
{
    struct flow_wildcards *wc = &ctx->xout->wc;
    struct flow *flow = &ctx->xin->flow;
    int n = flow_count_mpls_labels(flow, wc);

    if (!flow_pop_mpls(flow, n, eth_type, wc) && n >= FLOW_MAX_MPLS_LABELS) {
        if (ctx->xin->packet != NULL) {
            static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 5);
            VLOG_WARN_RL(&rl, "bridge %s: dropping packet on which an "
                         "MPLS pop action can't be performed as it has "
                         "more MPLS LSEs than the %d supported.",
                         ctx->xbridge->name, FLOW_MAX_MPLS_LABELS);
        }
        ctx->exit = true;
        ofpbuf_clear(&ctx->xout->odp_actions);
    }
}

static bool
compose_dec_ttl(struct xlate_ctx *ctx, struct ofpact_cnt_ids *ids)
{
    struct flow *flow = &ctx->xin->flow;

    if (!is_ip_any(flow)) {
        return false;
    }

    ctx->xout->wc.masks.nw_ttl = 0xff;
    if (flow->nw_ttl > 1) {
        flow->nw_ttl--;
        return false;
    } else {
        size_t i;

        for (i = 0; i < ids->n_controllers; i++) {
            execute_controller_action(ctx, UINT16_MAX, OFPR_INVALID_TTL,
                                      ids->cnt_ids[i]);
        }

        /* Stop processing for current table. */
        return true;
    }
}

static void
compose_set_mpls_label_action(struct xlate_ctx *ctx, ovs_be32 label)
{
    if (eth_type_mpls(ctx->xin->flow.dl_type)) {
        ctx->xout->wc.masks.mpls_lse[0] |= htonl(MPLS_LABEL_MASK);
        set_mpls_lse_label(&ctx->xin->flow.mpls_lse[0], label);
    }
}

static void
compose_set_mpls_tc_action(struct xlate_ctx *ctx, uint8_t tc)
{
    if (eth_type_mpls(ctx->xin->flow.dl_type)) {
        ctx->xout->wc.masks.mpls_lse[0] |= htonl(MPLS_TC_MASK);
        set_mpls_lse_tc(&ctx->xin->flow.mpls_lse[0], tc);
    }
}

static void
compose_set_mpls_ttl_action(struct xlate_ctx *ctx, uint8_t ttl)
{
    if (eth_type_mpls(ctx->xin->flow.dl_type)) {
        ctx->xout->wc.masks.mpls_lse[0] |= htonl(MPLS_TTL_MASK);
        set_mpls_lse_ttl(&ctx->xin->flow.mpls_lse[0], ttl);
    }
}

static bool
compose_dec_mpls_ttl_action(struct xlate_ctx *ctx)
{
    struct flow *flow = &ctx->xin->flow;
    uint8_t ttl = mpls_lse_to_ttl(flow->mpls_lse[0]);
    struct flow_wildcards *wc = &ctx->xout->wc;

    memset(&wc->masks.mpls_lse, 0xff, sizeof wc->masks.mpls_lse);
    if (eth_type_mpls(flow->dl_type)) {
        if (ttl > 1) {
            ttl--;
            set_mpls_lse_ttl(&flow->mpls_lse[0], ttl);
            return false;
        } else {
            execute_controller_action(ctx, UINT16_MAX, OFPR_INVALID_TTL, 0);

            /* Stop processing for current table. */
            return true;
        }
    } else {
        return true;
    }
}

static void
xlate_output_action(struct xlate_ctx *ctx,
                    ofp_port_t port, uint16_t max_len, bool may_packet_in)
{
    ofp_port_t prev_nf_output_iface = ctx->xout->nf_output_iface;

    ctx->xout->nf_output_iface = NF_OUT_DROP;

    switch (port) {
    case OFPP_IN_PORT:
        compose_output_action(ctx, ctx->xin->flow.in_port.ofp_port);
        break;
    case OFPP_TABLE:
        xlate_table_action(ctx, ctx->xin->flow.in_port.ofp_port,
                           0, may_packet_in, true);
        break;
    case OFPP_NORMAL:
        xlate_normal(ctx);
        break;
    case OFPP_FLOOD:
        flood_packets(ctx,  false);
        break;
    case OFPP_ALL:
        flood_packets(ctx, true);
        break;
    case OFPP_CONTROLLER:
        execute_controller_action(ctx, max_len, OFPR_ACTION, 0);
        break;
    case OFPP_NONE:
        break;
    case OFPP_LOCAL:
    default:
        if (port != ctx->xin->flow.in_port.ofp_port) {
            compose_output_action(ctx, port);
        } else {
            xlate_report(ctx, "skipping output to input port");
        }
        break;
    }

    if (prev_nf_output_iface == NF_OUT_FLOOD) {
        ctx->xout->nf_output_iface = NF_OUT_FLOOD;
    } else if (ctx->xout->nf_output_iface == NF_OUT_DROP) {
        ctx->xout->nf_output_iface = prev_nf_output_iface;
    } else if (prev_nf_output_iface != NF_OUT_DROP &&
               ctx->xout->nf_output_iface != NF_OUT_FLOOD) {
        ctx->xout->nf_output_iface = NF_OUT_MULTI;
    }
}

static void
xlate_output_reg_action(struct xlate_ctx *ctx,
                        const struct ofpact_output_reg *or)
{
    uint64_t port = mf_get_subfield(&or->src, &ctx->xin->flow);
    if (port <= UINT16_MAX) {
        union mf_subvalue value;

        memset(&value, 0xff, sizeof value);
        mf_write_subfield_flow(&or->src, &value, &ctx->xout->wc.masks);
        xlate_output_action(ctx, u16_to_ofp(port),
                            or->max_len, false);
    }
}

static void
xlate_enqueue_action(struct xlate_ctx *ctx,
                     const struct ofpact_enqueue *enqueue)
{
    ofp_port_t ofp_port = enqueue->port;
    uint32_t queue_id = enqueue->queue;
    uint32_t flow_priority, priority;
    int error;

    /* Translate queue to priority. */
    error = dpif_queue_to_priority(ctx->xbridge->dpif, queue_id, &priority);
    if (error) {
        /* Fall back to ordinary output action. */
        xlate_output_action(ctx, enqueue->port, 0, false);
        return;
    }

    /* Check output port. */
    if (ofp_port == OFPP_IN_PORT) {
        ofp_port = ctx->xin->flow.in_port.ofp_port;
    } else if (ofp_port == ctx->xin->flow.in_port.ofp_port) {
        return;
    }

    /* Add datapath actions. */
    flow_priority = ctx->xin->flow.skb_priority;
    ctx->xin->flow.skb_priority = priority;
    compose_output_action(ctx, ofp_port);
    ctx->xin->flow.skb_priority = flow_priority;

    /* Update NetFlow output port. */
    if (ctx->xout->nf_output_iface == NF_OUT_DROP) {
        ctx->xout->nf_output_iface = ofp_port;
    } else if (ctx->xout->nf_output_iface != NF_OUT_FLOOD) {
        ctx->xout->nf_output_iface = NF_OUT_MULTI;
    }
}

static void
xlate_set_queue_action(struct xlate_ctx *ctx, uint32_t queue_id)
{
    uint32_t skb_priority;

    if (!dpif_queue_to_priority(ctx->xbridge->dpif, queue_id, &skb_priority)) {
        ctx->xin->flow.skb_priority = skb_priority;
    } else {
        /* Couldn't translate queue to a priority.  Nothing to do.  A warning
         * has already been logged. */
    }
}

static bool
slave_enabled_cb(ofp_port_t ofp_port, void *xbridge_)
{
    const struct xbridge *xbridge = xbridge_;
    struct xport *port;

    switch (ofp_port) {
    case OFPP_IN_PORT:
    case OFPP_TABLE:
    case OFPP_NORMAL:
    case OFPP_FLOOD:
    case OFPP_ALL:
    case OFPP_NONE:
        return true;
    case OFPP_CONTROLLER: /* Not supported by the bundle action. */
        return false;
    default:
        port = get_ofp_port(xbridge, ofp_port);
        return port ? port->may_enable : false;
    }
}

static void
xlate_bundle_action(struct xlate_ctx *ctx,
                    const struct ofpact_bundle *bundle)
{
    ofp_port_t port;

    port = bundle_execute(bundle, &ctx->xin->flow, &ctx->xout->wc,
                          slave_enabled_cb,
                          CONST_CAST(struct xbridge *, ctx->xbridge));
    if (bundle->dst.field) {
        nxm_reg_load(&bundle->dst, ofp_to_u16(port), &ctx->xin->flow,
                     &ctx->xout->wc);
    } else {
        xlate_output_action(ctx, port, 0, false);
    }
}

static void
xlate_learn_action__(struct xlate_ctx *ctx, const struct ofpact_learn *learn,
                     struct ofputil_flow_mod *fm, struct ofpbuf *ofpacts)
{
    learn_execute(learn, &ctx->xin->flow, fm, ofpacts);
    if (ctx->xin->may_learn) {
        ofproto_dpif_flow_mod(ctx->xbridge->ofproto, fm);
    }
}

static void
xlate_learn_action(struct xlate_ctx *ctx, const struct ofpact_learn *learn)
{
    ctx->xout->has_learn = true;
    learn_mask(learn, &ctx->xout->wc);

    if (ctx->xin->xcache) {
        struct xc_entry *entry;

        entry = xlate_cache_add_entry(ctx->xin->xcache, XC_LEARN);
        entry->u.learn.ofproto = ctx->xbridge->ofproto;
        entry->u.learn.fm = xmalloc(sizeof *entry->u.learn.fm);
        entry->u.learn.ofpacts = ofpbuf_new(64);
        xlate_learn_action__(ctx, learn, entry->u.learn.fm,
                             entry->u.learn.ofpacts);
    } else if (ctx->xin->may_learn) {
        uint64_t ofpacts_stub[1024 / 8];
        struct ofputil_flow_mod fm;
        struct ofpbuf ofpacts;

        ofpbuf_use_stub(&ofpacts, ofpacts_stub, sizeof ofpacts_stub);
        xlate_learn_action__(ctx, learn, &fm, &ofpacts);
        ofpbuf_uninit(&ofpacts);
    }
}

static void
xlate_fin_timeout__(struct rule_dpif *rule, uint16_t tcp_flags,
                    uint16_t idle_timeout, uint16_t hard_timeout)
{
    if (tcp_flags & (TCP_FIN | TCP_RST)) {
        rule_dpif_reduce_timeouts(rule, idle_timeout, hard_timeout);
    }
}

static void
xlate_fin_timeout(struct xlate_ctx *ctx,
                  const struct ofpact_fin_timeout *oft)
{
    if (ctx->rule) {
        xlate_fin_timeout__(ctx->rule, ctx->xin->tcp_flags,
                            oft->fin_idle_timeout, oft->fin_hard_timeout);
        if (ctx->xin->xcache) {
            struct xc_entry *entry;

            entry = xlate_cache_add_entry(ctx->xin->xcache, XC_FIN_TIMEOUT);
            /* XC_RULE already holds a reference on the rule, none is taken
             * here. */
            entry->u.fin.rule = ctx->rule;
            entry->u.fin.idle = oft->fin_idle_timeout;
            entry->u.fin.hard = oft->fin_hard_timeout;
        }
    }
}

static void
xlate_sample_action(struct xlate_ctx *ctx,
                    const struct ofpact_sample *os)
{
  union user_action_cookie cookie;
  /* Scale the probability from 16-bit to 32-bit while representing
   * the same percentage. */
  uint32_t probability = (os->probability << 16) | os->probability;

  if (!ctx->xbridge->variable_length_userdata) {
      static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 1);

      VLOG_ERR_RL(&rl, "ignoring NXAST_SAMPLE action because datapath "
                  "lacks support (needs Linux 3.10+ or kernel module from "
                  "OVS 1.11+)");
      return;
  }

  ctx->xout->slow |= commit_odp_actions(&ctx->xin->flow, &ctx->base_flow,
                                        &ctx->xout->odp_actions,
                                        &ctx->xout->wc);

  compose_flow_sample_cookie(os->probability, os->collector_set_id,
                             os->obs_domain_id, os->obs_point_id, &cookie);
  compose_sample_action(ctx->xbridge, &ctx->xout->odp_actions, &ctx->xin->flow,
                        probability, &cookie, sizeof cookie.flow_sample);
}

static bool
may_receive(const struct xport *xport, struct xlate_ctx *ctx)
{
    if (xport->config & (is_stp(&ctx->xin->flow)
                         ? OFPUTIL_PC_NO_RECV_STP
                         : OFPUTIL_PC_NO_RECV)) {
        return false;
    }

    /* Only drop packets here if both forwarding and learning are
     * disabled.  If just learning is enabled, we need to have
     * OFPP_NORMAL and the learning action have a look at the packet
     * before we can drop it. */
    if (!xport_stp_forward_state(xport) && !xport_stp_learn_state(xport)) {
        return false;
    }

    return true;
}

static void
xlate_write_actions(struct xlate_ctx *ctx, const struct ofpact *a)
{
    struct ofpact_nest *on = ofpact_get_WRITE_ACTIONS(a);
    ofpbuf_put(&ctx->action_set, on->actions, ofpact_nest_get_action_len(on));
    ofpact_pad(&ctx->action_set);
}

static void
xlate_action_set(struct xlate_ctx *ctx)
{
    uint64_t action_list_stub[1024 / 64];
    struct ofpbuf action_list;

    ofpbuf_use_stub(&action_list, action_list_stub, sizeof action_list_stub);
    ofpacts_execute_action_set(&action_list, &ctx->action_set);
    do_xlate_actions(ofpbuf_data(&action_list), ofpbuf_size(&action_list), ctx);
    ofpbuf_uninit(&action_list);
}

static void
do_xlate_actions(const struct ofpact *ofpacts, size_t ofpacts_len,
                 struct xlate_ctx *ctx)
{
    struct flow_wildcards *wc = &ctx->xout->wc;
    struct flow *flow = &ctx->xin->flow;
    const struct ofpact *a;

    /* dl_type already in the mask, not set below. */

    OFPACT_FOR_EACH (a, ofpacts, ofpacts_len) {
        struct ofpact_controller *controller;
        const struct ofpact_metadata *metadata;
        const struct ofpact_set_field *set_field;
        const struct mf_field *mf;

        if (ctx->exit) {
            break;
        }

        switch (a->type) {
        case OFPACT_OUTPUT:
            xlate_output_action(ctx, ofpact_get_OUTPUT(a)->port,
                                ofpact_get_OUTPUT(a)->max_len, true);
            break;

        case OFPACT_GROUP:
            if (xlate_group_action(ctx, ofpact_get_GROUP(a)->group_id)) {
                return;
            }
            break;

        case OFPACT_CONTROLLER:
            controller = ofpact_get_CONTROLLER(a);
            execute_controller_action(ctx, controller->max_len,
                                      controller->reason,
                                      controller->controller_id);
            break;

        case OFPACT_ENQUEUE:
            xlate_enqueue_action(ctx, ofpact_get_ENQUEUE(a));
            break;

        case OFPACT_SET_VLAN_VID:
            wc->masks.vlan_tci |= htons(VLAN_VID_MASK | VLAN_CFI);
            if (flow->vlan_tci & htons(VLAN_CFI) ||
                ofpact_get_SET_VLAN_VID(a)->push_vlan_if_needed) {
                flow->vlan_tci &= ~htons(VLAN_VID_MASK);
                flow->vlan_tci |= (htons(ofpact_get_SET_VLAN_VID(a)->vlan_vid)
                                   | htons(VLAN_CFI));
            }
            break;

        case OFPACT_SET_VLAN_PCP:
            wc->masks.vlan_tci |= htons(VLAN_PCP_MASK | VLAN_CFI);
            if (flow->vlan_tci & htons(VLAN_CFI) ||
                ofpact_get_SET_VLAN_PCP(a)->push_vlan_if_needed) {
                flow->vlan_tci &= ~htons(VLAN_PCP_MASK);
                flow->vlan_tci |= htons((ofpact_get_SET_VLAN_PCP(a)->vlan_pcp
                                         << VLAN_PCP_SHIFT) | VLAN_CFI);
            }
            break;

        case OFPACT_STRIP_VLAN:
            memset(&wc->masks.vlan_tci, 0xff, sizeof wc->masks.vlan_tci);
            flow->vlan_tci = htons(0);
            break;

        case OFPACT_PUSH_VLAN:
            /* XXX 802.1AD(QinQ) */
            memset(&wc->masks.vlan_tci, 0xff, sizeof wc->masks.vlan_tci);
            flow->vlan_tci = htons(VLAN_CFI);
            break;

        case OFPACT_SET_ETH_SRC:
            memset(&wc->masks.dl_src, 0xff, sizeof wc->masks.dl_src);
            memcpy(flow->dl_src, ofpact_get_SET_ETH_SRC(a)->mac, ETH_ADDR_LEN);
            break;

        case OFPACT_SET_ETH_DST:
            memset(&wc->masks.dl_dst, 0xff, sizeof wc->masks.dl_dst);
            memcpy(flow->dl_dst, ofpact_get_SET_ETH_DST(a)->mac, ETH_ADDR_LEN);
            break;

        case OFPACT_SET_IPV4_SRC:
            if (flow->dl_type == htons(ETH_TYPE_IP)) {
                memset(&wc->masks.nw_src, 0xff, sizeof wc->masks.nw_src);
                flow->nw_src = ofpact_get_SET_IPV4_SRC(a)->ipv4;
            }
            break;

        case OFPACT_SET_IPV4_DST:
            if (flow->dl_type == htons(ETH_TYPE_IP)) {
                memset(&wc->masks.nw_dst, 0xff, sizeof wc->masks.nw_dst);
                flow->nw_dst = ofpact_get_SET_IPV4_DST(a)->ipv4;
            }
            break;

        case OFPACT_SET_IP_DSCP:
            if (is_ip_any(flow)) {
                wc->masks.nw_tos |= IP_DSCP_MASK;
                flow->nw_tos &= ~IP_DSCP_MASK;
                flow->nw_tos |= ofpact_get_SET_IP_DSCP(a)->dscp;
            }
            break;

        case OFPACT_SET_IP_ECN:
            if (is_ip_any(flow)) {
                wc->masks.nw_tos |= IP_ECN_MASK;
                flow->nw_tos &= ~IP_ECN_MASK;
                flow->nw_tos |= ofpact_get_SET_IP_ECN(a)->ecn;
            }
            break;

        case OFPACT_SET_IP_TTL:
            if (is_ip_any(flow)) {
                wc->masks.nw_ttl = 0xff;
                flow->nw_ttl = ofpact_get_SET_IP_TTL(a)->ttl;
            }
            break;

        case OFPACT_SET_L4_SRC_PORT:
            if (is_ip_any(flow)) {
                memset(&wc->masks.nw_proto, 0xff, sizeof wc->masks.nw_proto);
                memset(&wc->masks.tp_src, 0xff, sizeof wc->masks.tp_src);
                flow->tp_src = htons(ofpact_get_SET_L4_SRC_PORT(a)->port);
            }
            break;

        case OFPACT_SET_L4_DST_PORT:
            if (is_ip_any(flow)) {
                memset(&wc->masks.nw_proto, 0xff, sizeof wc->masks.nw_proto);
                memset(&wc->masks.tp_dst, 0xff, sizeof wc->masks.tp_dst);
                flow->tp_dst = htons(ofpact_get_SET_L4_DST_PORT(a)->port);
            }
            break;

        case OFPACT_RESUBMIT:
            xlate_ofpact_resubmit(ctx, ofpact_get_RESUBMIT(a));
            break;

        case OFPACT_SET_TUNNEL:
            flow->tunnel.tun_id = htonll(ofpact_get_SET_TUNNEL(a)->tun_id);
            break;

        case OFPACT_SET_QUEUE:
            xlate_set_queue_action(ctx, ofpact_get_SET_QUEUE(a)->queue_id);
            break;

        case OFPACT_POP_QUEUE:
            flow->skb_priority = ctx->orig_skb_priority;
            break;

        case OFPACT_REG_MOVE:
            nxm_execute_reg_move(ofpact_get_REG_MOVE(a), flow, wc);
            break;

        case OFPACT_REG_LOAD:
            nxm_execute_reg_load(ofpact_get_REG_LOAD(a), flow, wc);
            break;

        case OFPACT_SET_FIELD:
            set_field = ofpact_get_SET_FIELD(a);
            mf = set_field->field;

            /* Set field action only ever overwrites packet's outermost
             * applicable header fields.  Do nothing if no header exists. */
            if (mf->id == MFF_VLAN_VID) {
                wc->masks.vlan_tci |= htons(VLAN_CFI);
                if (!(flow->vlan_tci & htons(VLAN_CFI))) {
                    break;
                }
            } else if ((mf->id == MFF_MPLS_LABEL || mf->id == MFF_MPLS_TC)
                       /* 'dl_type' is already unwildcarded. */
                       && !eth_type_mpls(flow->dl_type)) {
                break;
            }

            mf_mask_field_and_prereqs(mf, &wc->masks);
            mf_set_flow_value(mf, &set_field->value, flow);
            break;

        case OFPACT_STACK_PUSH:
            nxm_execute_stack_push(ofpact_get_STACK_PUSH(a), flow, wc,
                                   &ctx->stack);
            break;

        case OFPACT_STACK_POP:
            nxm_execute_stack_pop(ofpact_get_STACK_POP(a), flow, wc,
                                  &ctx->stack);
            break;

        case OFPACT_PUSH_MPLS:
            compose_mpls_push_action(ctx, ofpact_get_PUSH_MPLS(a));
            break;

        case OFPACT_POP_MPLS:
            compose_mpls_pop_action(ctx, ofpact_get_POP_MPLS(a)->ethertype);
            break;

        case OFPACT_SET_MPLS_LABEL:
            compose_set_mpls_label_action(
                ctx, ofpact_get_SET_MPLS_LABEL(a)->label);
        break;

        case OFPACT_SET_MPLS_TC:
            compose_set_mpls_tc_action(ctx, ofpact_get_SET_MPLS_TC(a)->tc);
            break;

        case OFPACT_SET_MPLS_TTL:
            compose_set_mpls_ttl_action(ctx, ofpact_get_SET_MPLS_TTL(a)->ttl);
            break;

        case OFPACT_DEC_MPLS_TTL:
            if (compose_dec_mpls_ttl_action(ctx)) {
                return;
            }
            break;

        case OFPACT_DEC_TTL:
            wc->masks.nw_ttl = 0xff;
            if (compose_dec_ttl(ctx, ofpact_get_DEC_TTL(a))) {
                return;
            }
            break;

        case OFPACT_NOTE:
            /* Nothing to do. */
            break;

        case OFPACT_MULTIPATH:
            multipath_execute(ofpact_get_MULTIPATH(a), flow, wc);
            break;

        case OFPACT_BUNDLE:
            xlate_bundle_action(ctx, ofpact_get_BUNDLE(a));
            break;

        case OFPACT_OUTPUT_REG:
            xlate_output_reg_action(ctx, ofpact_get_OUTPUT_REG(a));
            break;

        case OFPACT_LEARN:
            xlate_learn_action(ctx, ofpact_get_LEARN(a));
            break;

        case OFPACT_EXIT:
            ctx->exit = true;
            break;

        case OFPACT_FIN_TIMEOUT:
            memset(&wc->masks.nw_proto, 0xff, sizeof wc->masks.nw_proto);
            ctx->xout->has_fin_timeout = true;
            xlate_fin_timeout(ctx, ofpact_get_FIN_TIMEOUT(a));
            break;

        case OFPACT_CLEAR_ACTIONS:
            ofpbuf_clear(&ctx->action_set);
            break;

        case OFPACT_WRITE_ACTIONS:
            xlate_write_actions(ctx, a);
            break;

        case OFPACT_WRITE_METADATA:
            metadata = ofpact_get_WRITE_METADATA(a);
            flow->metadata &= ~metadata->mask;
            flow->metadata |= metadata->metadata & metadata->mask;
            break;

        case OFPACT_METER:
            /* Not implemented yet. */
            break;

        case OFPACT_GOTO_TABLE: {
            struct ofpact_goto_table *ogt = ofpact_get_GOTO_TABLE(a);

            ovs_assert(ctx->table_id < ogt->table_id);
            xlate_table_action(ctx, ctx->xin->flow.in_port.ofp_port,
                               ogt->table_id, true, true);
            break;
        }

        case OFPACT_SAMPLE:
            xlate_sample_action(ctx, ofpact_get_SAMPLE(a));
            break;
        }
    }
}

void
xlate_in_init(struct xlate_in *xin, struct ofproto_dpif *ofproto,
              const struct flow *flow, struct rule_dpif *rule,
              uint16_t tcp_flags, const struct ofpbuf *packet)
{
    xin->ofproto = ofproto;
    xin->flow = *flow;
    xin->packet = packet;
    xin->may_learn = packet != NULL;
    xin->rule = rule;
    xin->xcache = NULL;
    xin->ofpacts = NULL;
    xin->ofpacts_len = 0;
    xin->tcp_flags = tcp_flags;
    xin->resubmit_hook = NULL;
    xin->report_hook = NULL;
    xin->resubmit_stats = NULL;
    xin->skip_wildcards = false;
}

void
xlate_out_uninit(struct xlate_out *xout)
{
    if (xout) {
        ofpbuf_uninit(&xout->odp_actions);
    }
}

/* Translates the 'ofpacts_len' bytes of "struct ofpact"s starting at 'ofpacts'
 * into datapath actions, using 'ctx', and discards the datapath actions. */
void
xlate_actions_for_side_effects(struct xlate_in *xin)
{
    struct xlate_out xout;

    xlate_actions(xin, &xout);
    xlate_out_uninit(&xout);
}

static void
xlate_report(struct xlate_ctx *ctx, const char *s)
{
    if (ctx->xin->report_hook) {
        ctx->xin->report_hook(ctx->xin, s, ctx->recurse);
    }
}

void
xlate_out_copy(struct xlate_out *dst, const struct xlate_out *src)
{
    dst->wc = src->wc;
    dst->slow = src->slow;
    dst->has_learn = src->has_learn;
    dst->has_normal = src->has_normal;
    dst->has_fin_timeout = src->has_fin_timeout;
    dst->nf_output_iface = src->nf_output_iface;
    dst->mirrors = src->mirrors;

    ofpbuf_use_stub(&dst->odp_actions, dst->odp_actions_stub,
                    sizeof dst->odp_actions_stub);
    ofpbuf_put(&dst->odp_actions, ofpbuf_data(&src->odp_actions),
               ofpbuf_size(&src->odp_actions));
}

static struct skb_priority_to_dscp *
get_skb_priority(const struct xport *xport, uint32_t skb_priority)
{
    struct skb_priority_to_dscp *pdscp;
    uint32_t hash;

    hash = hash_int(skb_priority, 0);
    HMAP_FOR_EACH_IN_BUCKET (pdscp, hmap_node, hash, &xport->skb_priorities) {
        if (pdscp->skb_priority == skb_priority) {
            return pdscp;
        }
    }
    return NULL;
}

static bool
dscp_from_skb_priority(const struct xport *xport, uint32_t skb_priority,
                       uint8_t *dscp)
{
    struct skb_priority_to_dscp *pdscp = get_skb_priority(xport, skb_priority);
    *dscp = pdscp ? pdscp->dscp : 0;
    return pdscp != NULL;
}

static void
clear_skb_priorities(struct xport *xport)
{
    struct skb_priority_to_dscp *pdscp, *next;

    HMAP_FOR_EACH_SAFE (pdscp, next, hmap_node, &xport->skb_priorities) {
        hmap_remove(&xport->skb_priorities, &pdscp->hmap_node);
        free(pdscp);
    }
}

static bool
actions_output_to_local_port(const struct xlate_ctx *ctx)
{
    odp_port_t local_odp_port = ofp_port_to_odp_port(ctx->xbridge, OFPP_LOCAL);
    const struct nlattr *a;
    unsigned int left;

    NL_ATTR_FOR_EACH_UNSAFE (a, left, ofpbuf_data(&ctx->xout->odp_actions),
                             ofpbuf_size(&ctx->xout->odp_actions)) {
        if (nl_attr_type(a) == OVS_ACTION_ATTR_OUTPUT
            && nl_attr_get_odp_port(a) == local_odp_port) {
            return true;
        }
    }
    return false;
}

/* Translates the 'ofpacts_len' bytes of "struct ofpacts" starting at 'ofpacts'
 * into datapath actions in 'odp_actions', using 'ctx'.
 *
 * The caller must take responsibility for eventually freeing 'xout', with
 * xlate_out_uninit(). */
void
xlate_actions(struct xlate_in *xin, struct xlate_out *xout)
{
    struct xlate_cfg *xcfg = ovsrcu_get(struct xlate_cfg *, &xcfgp);
    struct flow_wildcards *wc = &xout->wc;
    struct flow *flow = &xin->flow;
    struct rule_dpif *rule = NULL;

    const struct rule_actions *actions = NULL;
    enum slow_path_reason special;
    const struct ofpact *ofpacts;
    struct xport *in_port;
    struct flow orig_flow;
    struct xlate_ctx ctx;
    size_t ofpacts_len;
    bool tnl_may_send;
    bool is_icmp;

    COVERAGE_INC(xlate_actions);

    /* Flow initialization rules:
     * - 'base_flow' must match the kernel's view of the packet at the
     *   time that action processing starts.  'flow' represents any
     *   transformations we wish to make through actions.
     * - By default 'base_flow' and 'flow' are the same since the input
     *   packet matches the output before any actions are applied.
     * - When using VLAN splinters, 'base_flow''s VLAN is set to the value
     *   of the received packet as seen by the kernel.  If we later output
     *   to another device without any modifications this will cause us to
     *   insert a new tag since the original one was stripped off by the
     *   VLAN device.
     * - Tunnel metadata as received is retained in 'flow'. This allows
     *   tunnel metadata matching also in later tables.
     *   Since a kernel action for setting the tunnel metadata will only be
     *   generated with actual tunnel output, changing the tunnel metadata
     *   values in 'flow' (such as tun_id) will only have effect with a later
     *   tunnel output action.
     * - Tunnel 'base_flow' is completely cleared since that is what the
     *   kernel does.  If we wish to maintain the original values an action
     *   needs to be generated. */

    ctx.xin = xin;
    ctx.xout = xout;
    ctx.xout->slow = 0;
    ctx.xout->has_learn = false;
    ctx.xout->has_normal = false;
    ctx.xout->has_fin_timeout = false;
    ctx.xout->nf_output_iface = NF_OUT_DROP;
    ctx.xout->mirrors = 0;
    ofpbuf_use_stub(&ctx.xout->odp_actions, ctx.xout->odp_actions_stub,
                    sizeof ctx.xout->odp_actions_stub);
    ofpbuf_reserve(&ctx.xout->odp_actions, NL_A_U32_SIZE);

    ctx.xbridge = xbridge_lookup(xcfg, xin->ofproto);
    if (!ctx.xbridge) {
        return;
    }

    ctx.rule = xin->rule;

    ctx.base_flow = *flow;
    memset(&ctx.base_flow.tunnel, 0, sizeof ctx.base_flow.tunnel);
    ctx.orig_tunnel_ip_dst = flow->tunnel.ip_dst;

    flow_wildcards_init_catchall(wc);
    memset(&wc->masks.in_port, 0xff, sizeof wc->masks.in_port);
    memset(&wc->masks.skb_priority, 0xff, sizeof wc->masks.skb_priority);
    memset(&wc->masks.dl_type, 0xff, sizeof wc->masks.dl_type);
    if (is_ip_any(flow)) {
        wc->masks.nw_frag |= FLOW_NW_FRAG_MASK;
    }
    is_icmp = is_icmpv4(flow) || is_icmpv6(flow);

    tnl_may_send = tnl_xlate_init(&ctx.base_flow, flow, wc);
    if (ctx.xbridge->netflow) {
        netflow_mask_wc(flow, wc);
    }

    ctx.recurse = 0;
    ctx.resubmits = 0;
    ctx.in_group = false;
    ctx.orig_skb_priority = flow->skb_priority;
    ctx.table_id = 0;
    ctx.exit = false;
    ctx.use_recirc = false;

    if (!xin->ofpacts && !ctx.rule) {
        ctx.table_id = rule_dpif_lookup(ctx.xbridge->ofproto, flow,
                                        !xin->skip_wildcards ? wc : NULL,
                                        &rule, ctx.xin->xcache != NULL,
                                        ctx.xin->resubmit_stats);
        if (ctx.xin->resubmit_stats) {
            rule_dpif_credit_stats(rule, ctx.xin->resubmit_stats);
        }
        if (ctx.xin->xcache) {
            struct xc_entry *entry;

            entry = xlate_cache_add_entry(ctx.xin->xcache, XC_RULE);
            entry->u.rule = rule;
        }
        ctx.rule = rule;
    }
    xout->fail_open = ctx.rule && rule_dpif_is_fail_open(ctx.rule);

    if (xin->ofpacts) {
        ofpacts = xin->ofpacts;
        ofpacts_len = xin->ofpacts_len;
    } else if (ctx.rule) {
        actions = rule_dpif_get_actions(ctx.rule);
        ofpacts = actions->ofpacts;
        ofpacts_len = actions->ofpacts_len;
    } else {
        OVS_NOT_REACHED();
    }

    ofpbuf_use_stub(&ctx.stack, ctx.init_stack, sizeof ctx.init_stack);
    ofpbuf_use_stub(&ctx.action_set,
                    ctx.action_set_stub, sizeof ctx.action_set_stub);

    if (mbridge_has_mirrors(ctx.xbridge->mbridge)) {
        /* Do this conditionally because the copy is expensive enough that it
         * shows up in profiles. */
        orig_flow = *flow;
    }

    if (flow->nw_frag & FLOW_NW_FRAG_ANY) {
        switch (ctx.xbridge->frag) {
        case OFPC_FRAG_NORMAL:
            /* We must pretend that transport ports are unavailable. */
            flow->tp_src = ctx.base_flow.tp_src = htons(0);
            flow->tp_dst = ctx.base_flow.tp_dst = htons(0);
            break;

        case OFPC_FRAG_DROP:
            return;

        case OFPC_FRAG_REASM:
            OVS_NOT_REACHED();

        case OFPC_FRAG_NX_MATCH:
            /* Nothing to do. */
            break;

        case OFPC_INVALID_TTL_TO_CONTROLLER:
            OVS_NOT_REACHED();
        }
    }

    in_port = get_ofp_port(ctx.xbridge, flow->in_port.ofp_port);
    if (in_port && in_port->is_tunnel) {
        if (ctx.xin->resubmit_stats) {
            netdev_vport_inc_rx(in_port->netdev, ctx.xin->resubmit_stats);
            if (in_port->bfd) {
                bfd_account_rx(in_port->bfd, ctx.xin->resubmit_stats);
            }
        }
        if (ctx.xin->xcache) {
            struct xc_entry *entry;

            entry = xlate_cache_add_entry(ctx.xin->xcache, XC_NETDEV);
            entry->u.dev.rx = netdev_ref(in_port->netdev);
            entry->u.dev.bfd = bfd_ref(in_port->bfd);
        }
    }

    special = process_special(&ctx, flow, in_port, ctx.xin->packet);
    if (special) {
        ctx.xout->slow |= special;
    } else {
        size_t sample_actions_len;

        if (flow->in_port.ofp_port
            != vsp_realdev_to_vlandev(ctx.xbridge->ofproto,
                                      flow->in_port.ofp_port,
                                      flow->vlan_tci)) {
            ctx.base_flow.vlan_tci = 0;
        }

        add_sflow_action(&ctx);
        add_ipfix_action(&ctx);
        sample_actions_len = ofpbuf_size(&ctx.xout->odp_actions);

        if (tnl_may_send && (!in_port || may_receive(in_port, &ctx))) {
            do_xlate_actions(ofpacts, ofpacts_len, &ctx);

            /* We've let OFPP_NORMAL and the learning action look at the
             * packet, so drop it now if forwarding is disabled. */
            if (in_port && !xport_stp_forward_state(in_port)) {
                ofpbuf_set_size(&ctx.xout->odp_actions, sample_actions_len);
            }
        }

        if (ofpbuf_size(&ctx.action_set)) {
            xlate_action_set(&ctx);
        }

        if (ctx.xbridge->has_in_band
            && in_band_must_output_to_local_port(flow)
            && !actions_output_to_local_port(&ctx)) {
            compose_output_action(&ctx, OFPP_LOCAL);
        }

        fix_sflow_action(&ctx);

        if (mbridge_has_mirrors(ctx.xbridge->mbridge)) {
            add_mirror_actions(&ctx, &orig_flow);
        }
    }

    if (nl_attr_oversized(ofpbuf_size(&ctx.xout->odp_actions))) {
        /* These datapath actions are too big for a Netlink attribute, so we
         * can't hand them to the kernel directly.  dpif_execute() can execute
         * them one by one with help, so just mark the result as SLOW_ACTION to
         * prevent the flow from being installed. */
        COVERAGE_INC(xlate_actions_oversize);
        ctx.xout->slow |= SLOW_ACTION;
    }

    if (mbridge_has_mirrors(ctx.xbridge->mbridge)) {
        if (ctx.xin->resubmit_stats) {
            mirror_update_stats(ctx.xbridge->mbridge, xout->mirrors,
                                ctx.xin->resubmit_stats->n_packets,
                                ctx.xin->resubmit_stats->n_bytes);
        }
        if (ctx.xin->xcache) {
            struct xc_entry *entry;

            entry = xlate_cache_add_entry(ctx.xin->xcache, XC_MIRROR);
            entry->u.mirror.mbridge = mbridge_ref(ctx.xbridge->mbridge);
            entry->u.mirror.mirrors = xout->mirrors;
        }
    }

    if (ctx.xbridge->netflow) {
        /* Only update netflow if we don't have controller flow.  We don't
         * report NetFlow expiration messages for such facets because they
         * are just part of the control logic for the network, not real
         * traffic. */
        if (ofpacts_len == 0
            || ofpacts->type != OFPACT_CONTROLLER
            || ofpact_next(ofpacts) < ofpact_end(ofpacts, ofpacts_len)) {
            if (ctx.xin->resubmit_stats) {
                netflow_flow_update(ctx.xbridge->netflow, flow,
                                    xout->nf_output_iface,
                                    ctx.xin->resubmit_stats);
            }
            if (ctx.xin->xcache) {
                struct xc_entry *entry;

                entry = xlate_cache_add_entry(ctx.xin->xcache, XC_NETFLOW);
                entry->u.nf.netflow = netflow_ref(ctx.xbridge->netflow);
                entry->u.nf.flow = xmemdup(flow, sizeof *flow);
                entry->u.nf.iface = xout->nf_output_iface;
            }
        }
    }

    ofpbuf_uninit(&ctx.stack);
    ofpbuf_uninit(&ctx.action_set);

    /* Clear the metadata and register wildcard masks, because we won't
     * use non-header fields as part of the cache. */
    flow_wildcards_clear_non_packet_fields(wc);

    /* ICMPv4 and ICMPv6 have 8-bit "type" and "code" fields.  struct flow uses
     * the low 8 bits of the 16-bit tp_src and tp_dst members to represent
     * these fields.  The datapath interface, on the other hand, represents
     * them with just 8 bits each.  This means that if the high 8 bits of the
     * masks for these fields somehow become set, then they will get chopped
     * off by a round trip through the datapath, and revalidation will spot
     * that as an inconsistency and delete the flow.  Avoid the problem here by
     * making sure that only the low 8 bits of either field can be unwildcarded
     * for ICMP.
     */
    if (is_icmp) {
        wc->masks.tp_src &= htons(UINT8_MAX);
        wc->masks.tp_dst &= htons(UINT8_MAX);
    }
}

/* Sends 'packet' out 'ofport'.
 * May modify 'packet'.
 * Returns 0 if successful, otherwise a positive errno value. */
int
xlate_send_packet(const struct ofport_dpif *ofport, struct ofpbuf *packet)
{
    struct xlate_cfg *xcfg = ovsrcu_get(struct xlate_cfg *, &xcfgp);
    struct xport *xport;
    struct ofpact_output output;
    struct flow flow;

    ofpact_init(&output.ofpact, OFPACT_OUTPUT, sizeof output);
    /* Use OFPP_NONE as the in_port to avoid special packet processing. */
    flow_extract(packet, NULL, &flow);
    flow.in_port.ofp_port = OFPP_NONE;

    xport = xport_lookup(xcfg, ofport);
    if (!xport) {
        return EINVAL;
    }
    output.port = xport->ofp_port;
    output.max_len = 0;

    return ofproto_dpif_execute_actions(xport->xbridge->ofproto, &flow, NULL,
                                        &output.ofpact, sizeof output,
                                        packet);
}

struct xlate_cache *
xlate_cache_new(void)
{
    struct xlate_cache *xcache = xmalloc(sizeof *xcache);

    ofpbuf_init(&xcache->entries, 512);
    return xcache;
}

static struct xc_entry *
xlate_cache_add_entry(struct xlate_cache *xcache, enum xc_type type)
{
    struct xc_entry *entry;

    entry = ofpbuf_put_zeros(&xcache->entries, sizeof *entry);
    entry->type = type;

    return entry;
}

static void
xlate_cache_netdev(struct xc_entry *entry, const struct dpif_flow_stats *stats)
{
    if (entry->u.dev.tx) {
        netdev_vport_inc_tx(entry->u.dev.tx, stats);
    }
    if (entry->u.dev.rx) {
        netdev_vport_inc_rx(entry->u.dev.rx, stats);
    }
    if (entry->u.dev.bfd) {
        bfd_account_rx(entry->u.dev.bfd, stats);
    }
}

static void
xlate_cache_normal(struct ofproto_dpif *ofproto, struct flow *flow, int vlan)
{
    struct xlate_cfg *xcfg = ovsrcu_get(struct xlate_cfg *, &xcfgp);
    struct xbridge *xbridge;
    struct xbundle *xbundle;
    struct flow_wildcards wc;

    xbridge = xbridge_lookup(xcfg, ofproto);
    if (!xbridge) {
        return;
    }

    xbundle = lookup_input_bundle(xbridge, flow->in_port.ofp_port, false,
                                  NULL);
    if (!xbundle) {
        return;
    }

    update_learning_table(xbridge, flow, &wc, vlan, xbundle);
}

/* Push stats and perform side effects of flow translation. */
void
xlate_push_stats(struct xlate_cache *xcache, bool may_learn,
                 const struct dpif_flow_stats *stats)
{
    struct xc_entry *entry;
    struct ofpbuf entries = xcache->entries;

    XC_ENTRY_FOR_EACH (entry, entries, xcache) {
        switch (entry->type) {
        case XC_RULE:
            rule_dpif_credit_stats(entry->u.rule, stats);
            break;
        case XC_BOND:
            bond_account(entry->u.bond.bond, entry->u.bond.flow,
                         entry->u.bond.vid, stats->n_bytes);
            break;
        case XC_NETDEV:
            xlate_cache_netdev(entry, stats);
            break;
        case XC_NETFLOW:
            netflow_flow_update(entry->u.nf.netflow, entry->u.nf.flow,
                                entry->u.nf.iface, stats);
            break;
        case XC_MIRROR:
            mirror_update_stats(entry->u.mirror.mbridge,
                                entry->u.mirror.mirrors,
                                stats->n_packets, stats->n_bytes);
            break;
        case XC_LEARN:
            if (may_learn) {
                ofproto_dpif_flow_mod(entry->u.learn.ofproto,
                                      entry->u.learn.fm);
            }
            break;
        case XC_NORMAL:
            xlate_cache_normal(entry->u.normal.ofproto, entry->u.normal.flow,
                               entry->u.normal.vlan);
            break;
        case XC_FIN_TIMEOUT:
            xlate_fin_timeout__(entry->u.fin.rule, stats->tcp_flags,
                                entry->u.fin.idle, entry->u.fin.hard);
            break;
        case XC_GROUP:
            group_dpif_credit_stats(entry->u.group.group, entry->u.group.bucket,
                                    stats);
            break;
        default:
            OVS_NOT_REACHED();
        }
    }
}

static void
xlate_dev_unref(struct xc_entry *entry)
{
    if (entry->u.dev.tx) {
        netdev_close(entry->u.dev.tx);
    }
    if (entry->u.dev.rx) {
        netdev_close(entry->u.dev.rx);
    }
    if (entry->u.dev.bfd) {
        bfd_unref(entry->u.dev.bfd);
    }
}

static void
xlate_cache_clear_netflow(struct netflow *netflow, struct flow *flow)
{
    netflow_flow_clear(netflow, flow);
    netflow_unref(netflow);
    free(flow);
}

void
xlate_cache_clear(struct xlate_cache *xcache)
{
    struct xc_entry *entry;
    struct ofpbuf entries;

    if (!xcache) {
        return;
    }

    XC_ENTRY_FOR_EACH (entry, entries, xcache) {
        switch (entry->type) {
        case XC_RULE:
            rule_dpif_unref(entry->u.rule);
            break;
        case XC_BOND:
            free(entry->u.bond.flow);
            bond_unref(entry->u.bond.bond);
            break;
        case XC_NETDEV:
            xlate_dev_unref(entry);
            break;
        case XC_NETFLOW:
            xlate_cache_clear_netflow(entry->u.nf.netflow, entry->u.nf.flow);
            break;
        case XC_MIRROR:
            mbridge_unref(entry->u.mirror.mbridge);
            break;
        case XC_LEARN:
            free(entry->u.learn.fm);
            ofpbuf_delete(entry->u.learn.ofpacts);
            break;
        case XC_NORMAL:
            free(entry->u.normal.flow);
            break;
        case XC_FIN_TIMEOUT:
            /* 'u.fin.rule' is always already held as a XC_RULE, which
             * has already released it's reference above. */
            break;
        case XC_GROUP:
            group_dpif_unref(entry->u.group.group);
            break;
        default:
            OVS_NOT_REACHED();
        }
    }

    ofpbuf_clear(&xcache->entries);
}

void
xlate_cache_delete(struct xlate_cache *xcache)
{
    xlate_cache_clear(xcache);
    ofpbuf_uninit(&xcache->entries);
    free(xcache);
}
