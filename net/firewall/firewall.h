/*
 * NEXUS OS - Firewall and Security Framework
 * net/firewall/firewall.h
 * 
 * Complete firewall with packet filtering, NAT, and stateful inspection
 */

#ifndef _NEXUS_FIREWALL_H
#define _NEXUS_FIREWALL_H

#include "../core/net.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         FIREWALL CONSTANTS                                */
/*===========================================================================*/

#define FIREWALL_MAX_RULES      4096
#define FIREWALL_MAX_CHAINS     64
#define FIREWALL_MAX_TABLES     16

/* Firewall Actions */
#define FW_ACTION_ACCEPT        0
#define FW_ACTION_DROP          1
#define FW_ACTION_REJECT        2
#define FW_ACTION_REDIRECT      3
#define FW_ACTION_MASQUERADE    4
#define FW_ACTION_LOG           5
#define FW_ACTION_RATE_LIMIT    6

/* Firewall Protocols */
#define FW_PROTO_ANY            0
#define FW_PROTO_TCP            6
#define FW_PROTO_UDP            17
#define FW_PROTO_ICMP           1
#define FW_PROTO_ICMPV6         58

/* Firewall Chains */
#define FW_CHAIN_INPUT          0
#define FW_CHAIN_OUTPUT         1
#define FW_CHAIN_FORWARD        2
#define FW_CHAIN_PREROUTING     3
#define FW_CHAIN_POSTROUTING    4

/* Firewall Tables */
#define FW_TABLE_FILTER         0
#define FW_TABLE_NAT            1
#define FW_TABLE_MANGLE         2
#define FW_TABLE_RAW            3
#define FW_TABLE_SECURITY       4

/* Connection States */
#define FW_STATE_NEW            0x01
#define FW_STATE_ESTABLISHED    0x02
#define FW_STATE_RELATED        0x04
#define FW_STATE_INVALID        0x08

/* Log Levels */
#define FW_LOG_EMERG            0
#define FW_LOG_ALERT            1
#define FW_LOG_CRIT             2
#define FW_LOG_ERR              3
#define FW_LOG_WARNING          4
#define FW_LOG_NOTICE           5
#define FW_LOG_INFO             6
#define FW_LOG_DEBUG            7

/*===========================================================================*/
/*                         FIREWALL STRUCTURES                               */
/*===========================================================================*/

/**
 * fw_addr_t - Firewall address specification
 */
typedef struct {
    union {
        u8 ipv4[4];
        u8 ipv6[16];
    };
    u8 prefix_len;
    bool is_ipv6;
} fw_addr_t;

/**
 * fw_port_range_t - Port range specification
 */
typedef struct {
    u16 start;
    u16 end;
} fw_port_range_t;

/**
 * fw_rule_t - Firewall rule
 */
typedef struct fw_rule {
    u32 id;
    char name[64];
    u32 table;
    u32 chain;
    
    /* Match criteria */
    fw_addr_t src_addr;
    fw_addr_t dst_addr;
    fw_port_range_t src_port;
    fw_port_range_t dst_port;
    u8 protocol;
    u8 state_mask;
    u32 interface_in;
    u32 interface_out;
    
    /* Action */
    u8 action;
    fw_addr_t redirect_addr;
    u16 redirect_port;
    
    /* Rate limiting */
    u32 rate_limit;
    u32 rate_burst;
    
    /* Logging */
    bool log;
    u8 log_level;
    char log_prefix[32];
    
    /* Counters */
    u64 packets_matched;
    u64 bytes_matched;
    
    /* Link to next rule */
    struct fw_rule *next;
} fw_rule_t;

/**
 * fw_chain_t - Firewall chain (list of rules)
 */
typedef struct fw_chain {
    char name[32];
    u32 id;
    u32 table;
    fw_rule_t *rules;
    u32 rule_count;
    u8 default_policy;
    u64 packets_processed;
    u64 bytes_processed;
    struct fw_chain *next;
} fw_chain_t;

/**
 * fw_table_t - Firewall table (collection of chains)
 */
typedef struct {
    char name[32];
    u32 id;
    fw_chain_t *chains;
    u32 chain_count;
} fw_table_t;

/**
 * fw_conntrack_t - Connection tracking entry
 */
typedef struct fw_conntrack {
    u32 id;
    fw_addr_t src_addr;
    fw_addr_t dst_addr;
    u16 src_port;
    u16 dst_port;
    u8 protocol;
    u8 state;
    u64 created;
    u64 last_seen;
    u64 timeout;
    u64 packets;
    u64 bytes;
    struct fw_conntrack *next;
    struct fw_conntrack *related;
} fw_conntrack_t;

/**
 * fw_stats_t - Firewall statistics
 */
typedef struct {
    u64 total_packets;
    u64 total_bytes;
    u64 accepted_packets;
    u64 accepted_bytes;
    u64 dropped_packets;
    u64 dropped_bytes;
    u64 rejected_packets;
    u64 nat_translations;
    u64 conntrack_entries;
    u64 rules_evaluated;
} fw_stats_t;

/*===========================================================================*/
/*                         FIREWALL API                                      */
/*===========================================================================*/

/* Initialization */
void firewall_init(void);
void firewall_shutdown(void);

/* Table/Chain operations */
int fw_table_create(const char *name);
int fw_chain_create(const char *table, const char *chain);
int fw_chain_set_policy(const char *table, const char *chain, u8 policy);

/* Rule operations */
int fw_rule_add(fw_rule_t *rule);
int fw_rule_delete(u32 rule_id);
int fw_rule_insert(u32 position, fw_rule_t *rule);
int fw_rule_replace(u32 rule_id, fw_rule_t *rule);
fw_rule_t *fw_rule_get(u32 rule_id);
int fw_rule_flush(const char *table, const char *chain);

/* Packet processing */
int fw_process_packet(struct sk_buff *skb, u32 chain);
int fw_check_packet(struct sk_buff *skb);

/* Connection tracking */
fw_conntrack_t *fw_conntrack_get(struct sk_buff *skb);
int fw_conntrack_create(struct sk_buff *skb);
int fw_conntrack_update(struct sk_buff *skb);
int fw_conntrack_delete(struct sk_buff *skb);
void fw_conntrack_cleanup(void);

/* NAT operations */
int fw_nat_masquerade(struct sk_buff *skb);
int fw_nat_redirect(struct sk_buff *skb, fw_addr_t *addr, u16 port);
int fw_nat_dnat(struct sk_buff *skb, fw_addr_t *addr, u16 port);
int fw_nat_snat(struct sk_buff *skb, fw_addr_t *addr, u16 port);

/* Logging */
void fw_log_packet(struct sk_buff *skb, u8 level, const char *prefix);
void fw_log_rule_hit(fw_rule_t *rule, struct sk_buff *skb);

/* Statistics */
void fw_get_stats(fw_stats_t *stats);
void fw_dump_rules(void);
void fw_dump_conntrack(void);

/* Security profiles */
int fw_profile_load(const char *name);
int fw_profile_save(const char *name);
int fw_profile_reset(void);

/* Predefined rules */
int fw_allow_established(void);
int fw_allow_loopback(void);
int fw_allow_icmp(void);
int fw_allow_port(u8 proto, u16 port);
int fw_block_all(void);

#endif /* _NEXUS_FIREWALL_H */
