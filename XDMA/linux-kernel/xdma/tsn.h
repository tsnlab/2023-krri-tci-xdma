#pragma once

#include <linux/skbuff.h>
#include <linux/pci.h>
#include <net/pkt_sched.h>

typedef uint64_t timestamp_t;
typedef uint64_t sysclock_t;

enum tsn_prio {
	TSN_PRIO_GPTP = 3,
	TSN_PRIO_VLAN = 5,
	TSN_PRIO_BE = 7,
};

enum tsn_fail_policy {
	TSN_FAIL_POLICY_DROP = 0,
	TSN_FAIL_POLICY_RETRY = 1,
};

struct tsn_vlan_hdr {
	uint16_t pid;
	uint8_t pcp:3;
	uint8_t dei:1;
	uint16_t vid:12;
} __attribute__((packed, scalar_storage_order("big-endian")));

uint8_t tsn_get_vlan_prio(const uint8_t* payload);
bool tsn_fill_metadata(struct pci_dev* pdev, timestamp_t now, uint64_t cycle_1s, struct sk_buff* skb);
void tsn_init_configs(struct pci_dev* config);
int tsn_set_qav(struct pci_dev* config, struct tc_cbs_qopt_offload* qopt);
int tsn_set_qbv(struct pci_dev* config, struct tc_taprio_qopt_offload* qopt);
