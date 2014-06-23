#ifndef __LINUX_UDP_WRAPPER_H
#define __LINUX_UDP_WRAPPER_H 1

#include_next <linux/udp.h>

#ifndef HAVE_SKBUFF_HEADER_HELPERS
static inline struct udphdr *udp_hdr(const struct sk_buff *skb)
{
	return (struct udphdr *)skb_transport_header(skb);
}
#endif /* HAVE_SKBUFF_HEADER_HELPERS */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
static inline void udp_encap_enable(void)
{
}
#endif
#endif
