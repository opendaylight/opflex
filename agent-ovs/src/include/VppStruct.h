#ifndef VPPSTRUCT_H
#define VPPSTRUCT_H

#define MPLS_IETF_MAX_LABEL               0xfffff;
#define MPLS_LABEL_INVALID (MPLS_IETF_MAX_LABEL+1);

#include <VppTypes.h>

namespace ovsagent {
  /**
   * Vpp Version struct
   * Contains information about VPP version, build date and build directory
   */
  struct VppVersion
  {
    std::string program;
    std::string version;
    std::string buildDate;
    std::string buildDirectory;
  };

  typedef union
  {
    u8 as_u8[4];
    u32 as_u32;
  } ip4_address;

  struct ip6_address
  {
    u16 as_u16[8];
  };

  struct ip46_address
  {
    u8 is_ipv6;
    ip4_address ip4;
    ip6_address ip6;
  };

  struct ip46_route
  {
    u32 next_hop_sw_if_index;
    u32 table_id;
    u32 classify_table_index;
    u32 next_hop_table_id;
    u8 create_vrf_if_needed;
    u8 is_add;
    u8 is_drop;
    u8 is_unreach;
    u8 is_prohibit;
    u8 is_ipv6; //TODO <- not needed really since ip46_address has this.
    u8 is_local;
    u8 is_classify;
    u8 is_multipath;
    u8 is_resolve_host;
    u8 is_resolve_attached;
    // Is last/not-last message in group of multiple add/del messages.
    u8 not_last;
    u8 next_hop_weight;
    u8 dst_address_length;
    ip46_address dst_address;
    u8 next_hop_address_set;
    ip46_address next_hop_address;
    u8 next_hop_n_out_labels;
    u32 next_hop_via_label;
    std::vector<u32> next_hop_out_label_stack;
  };

  struct VppAclRule
  {
    u8 is_permit;
    u8 is_ipv6; //TODO redundant since IPv6 flag is in ip46 struct
    ip46_address src_ip_addr;
    u8 src_ip_prefix_len;
    ip46_address dst_ip_addr;
    u8 dst_ip_prefix_len;
    u8 proto;
    //  After an ip[46]_header_t, either:
    // (L4) u16 srcport : u16 dstport
    // (ICMP) u8 type : u8 code: u16 checksum
    u16 srcport_or_icmptype_first;
    u16 srcport_or_icmptype_last;
    u16 dstport_or_icmpcode_first;
    u16 dstport_or_icmpcode_last;
    u8 tcp_flags_mask;
    u8 tcp_flags_value;

  };

  using VppAclRules_t = std::vector<VppAclRule>;

  struct VppAcl
  {
    u32 acl_index;
    std::string tag;
    u32 count;
    VppAclRules_t r;
  };


  using VppAcls_t = std::vector<VppAcl>;

} //namespace ovsagent
#endif /* VPPSTRUCT_H */
