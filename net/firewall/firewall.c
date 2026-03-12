/*
 * NEXUS OS - Firewall Implementation
 * net/firewall/firewall.c
 * 
 * Stateful packet filtering with NAT and connection tracking
 */

#include "firewall.h"

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

static fw_table_t fw_tables[FIREWALL_MAX_TABLES];
static fw_conntrack_t *fw_conntrack_table = NULL;
static fw_stats_t fw_statistics;
static spinlock_t fw_lock;
static u32 fw_rule_counter = 0;
static u32 fw_conntrack_counter = 0;

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

/**
 * firewall_init - Initialize firewall subsystem
 */
void firewall_init(void)
{
    printk("[FIREWALL] Initializing firewall subsystem...\n");
    
    spin_lock_init(&fw_lock);
    memset(fw_tables, 0, sizeof(fw_tables));
    memset(&fw_statistics, 0, sizeof(fw_statistics));
    
    /* Create default tables */
    fw_table_create("filter");
    fw_table_create("nat");
    fw_table_create("mangle");
    fw_table_create("raw");
    fw_table_create("security");
    
    /* Create default chains */
    fw_chain_create("filter", "INPUT");
    fw_chain_create("filter", "OUTPUT");
    fw_chain_create("filter", "FORWARD");
    
    fw_chain_create("nat", "PREROUTING");
    fw_chain_create("nat", "POSTROUTING");
    
    fw_chain_create("mangle", "PREROUTING");
    fw_chain_create("mangle", "POSTROUTING");
    
    /* Set default policies */
    fw_chain_set_policy("filter", "INPUT", FW_ACTION_ACCEPT);
    fw_chain_set_policy("filter", "OUTPUT", FW_ACTION_ACCEPT);
    fw_chain_set_policy("filter", "FORWARD", FW_ACTION_DROP);
    fw_chain_set_policy("nat", "PREROUTING", FW_ACTION_ACCEPT);
    fw_chain_set_policy("nat", "POSTROUTING", FW_ACTION_ACCEPT);
    
    /* Add basic security rules */
    fw_allow_loopback();
    fw_allow_established();
    
    printk("[FIREWALL] Initialized with %d tables\n", FIREWALL_MAX_TABLES);
}

/**
 * firewall_shutdown - Shutdown firewall subsystem
 */
void firewall_shutdown(void)
{
    printk("[FIREWALL] Shutting down...\n");
    fw_conntrack_cleanup();
}

/*===========================================================================*/
/*                         TABLE/CHAIN OPERATIONS                            */
/*===========================================================================*/

/**
 * fw_table_create - Create firewall table
 */
int fw_table_create(const char *name)
{
    if (!name) return -1;
    
    for (u32 i = 0; i < FIREWALL_MAX_TABLES; i++) {
        if (fw_tables[i].id == 0) {
            strncpy(fw_tables[i].name, name, sizeof(fw_tables[i].name) - 1);
            fw_tables[i].id = i + 1;
            fw_tables[i].chains = NULL;
            fw_tables[i].chain_count = 0;
            printk("[FIREWALL] Created table: %s\n", name);
            return 0;
        }
    }
    
    return -1;
}

/**
 * fw_chain_create - Create firewall chain
 */
int fw_chain_create(const char *table, const char *chain)
{
    if (!table || !chain) return -1;
    
    /* Find table */
    fw_table_t *tbl = NULL;
    for (u32 i = 0; i < FIREWALL_MAX_TABLES; i++) {
        if (strcmp(fw_tables[i].name, table) == 0) {
            tbl = &fw_tables[i];
            break;
        }
    }
    
    if (!tbl) return -1;
    
    /* Create chain */
    fw_chain_t *ch = kmalloc(sizeof(fw_chain_t));
    if (!ch) return -ENOMEM;
    
    strncpy(ch->name, chain, sizeof(ch->name) - 1);
    ch->id = tbl->chain_count + 1;
    ch->table = tbl->id;
    ch->rules = NULL;
    ch->rule_count = 0;
    ch->default_policy = FW_ACTION_ACCEPT;
    ch->packets_processed = 0;
    ch->bytes_processed = 0;
    
    /* Add to table */
    ch->next = tbl->chains;
    tbl->chains = ch;
    tbl->chain_count++;
    
    printk("[FIREWALL] Created chain: %s/%s\n", table, chain);
    return 0;
}

/**
 * fw_chain_set_policy - Set chain default policy
 */
int fw_chain_set_policy(const char *table, const char *chain, u8 policy)
{
    if (!table || !chain) return -1;
    
    /* Find table and chain */
    for (u32 i = 0; i < FIREWALL_MAX_TABLES; i++) {
        if (strcmp(fw_tables[i].name, table) == 0) {
            fw_chain_t *ch = fw_tables[i].chains;
            while (ch) {
                if (strcmp(ch->name, chain) == 0) {
                    ch->default_policy = policy;
                    printk("[FIREWALL] Set policy %s/%s = %d\n", table, chain, policy);
                    return 0;
                }
                ch = ch->next;
            }
        }
    }
    
    return -1;
}

/*===========================================================================*/
/*                         RULE OPERATIONS                                   */
/*===========================================================================*/

/**
 * fw_rule_add - Add firewall rule
 */
int fw_rule_add(fw_rule_t *rule)
{
    if (!rule) return -1;
    
    spin_lock(&fw_lock);
    
    /* Allocate and copy rule */
    fw_rule_t *new_rule = kmalloc(sizeof(fw_rule_t));
    if (!new_rule) {
        spin_unlock(&fw_lock);
        return -ENOMEM;
    }
    
    memcpy(new_rule, rule, sizeof(fw_rule_t));
    new_rule->id = ++fw_rule_counter;
    new_rule->next = NULL;
    
    /* Find chain and add rule */
    for (u32 i = 0; i < FIREWALL_MAX_TABLES; i++) {
        if (fw_tables[i].id == rule->table) {
            fw_chain_t *ch = fw_tables[i].chains;
            while (ch) {
                if (ch->id == rule->chain) {
                    new_rule->next = ch->rules;
                    ch->rules = new_rule;
                    ch->rule_count++;
                    spin_unlock(&fw_lock);
                    printk("[FIREWALL] Added rule %u: %s\n", new_rule->id, rule->name);
                    return new_rule->id;
                }
                ch = ch->next;
            }
        }
    }
    
    kfree(new_rule);
    spin_unlock(&fw_lock);
    return -1;
}

/**
 * fw_rule_delete - Delete firewall rule
 */
int fw_rule_delete(u32 rule_id)
{
    spin_lock(&fw_lock);
    
    for (u32 i = 0; i < FIREWALL_MAX_TABLES; i++) {
        fw_chain_t *ch = fw_tables[i].chains;
        while (ch) {
            fw_rule_t *rule = ch->rules;
            fw_rule_t *prev = NULL;
            
            while (rule) {
                if (rule->id == rule_id) {
                    if (prev) {
                        prev->next = rule->next;
                    } else {
                        ch->rules = rule->next;
                    }
                    ch->rule_count--;
                    kfree(rule);
                    spin_unlock(&fw_lock);
                    printk("[FIREWALL] Deleted rule %u\n", rule_id);
                    return 0;
                }
                prev = rule;
                rule = rule->next;
            }
            ch = ch->next;
        }
    }
    
    spin_unlock(&fw_lock);
    return -1;
}

/**
 * fw_rule_flush - Flush all rules in chain
 */
int fw_rule_flush(const char *table, const char *chain)
{
    if (!table || !chain) return -1;
    
    spin_lock(&fw_lock);
    
    for (u32 i = 0; i < FIREWALL_MAX_TABLES; i++) {
        if (strcmp(fw_tables[i].name, table) == 0) {
            fw_chain_t *ch = fw_tables[i].chains;
            while (ch) {
                if (strcmp(ch->name, chain) == 0) {
                    /* Free all rules */
                    fw_rule_t *rule = ch->rules;
                    while (rule) {
                        fw_rule_t *next = rule->next;
                        kfree(rule);
                        rule = next;
                    }
                    ch->rules = NULL;
                    ch->rule_count = 0;
                    spin_unlock(&fw_lock);
                    printk("[FIREWALL] Flushed chain %s/%s\n", table, chain);
                    return 0;
                }
                ch = ch->next;
            }
        }
    }
    
    spin_unlock(&fw_lock);
    return -1;
}

/*===========================================================================*/
/*                         PACKET PROCESSING                                 */
/*===========================================================================*/

/**
 * fw_process_packet - Process packet through firewall
 */
int fw_process_packet(struct sk_buff *skb, u32 chain)
{
    if (!skb) return FW_ACTION_DROP;
    
    spin_lock(&fw_lock);
    
    fw_statistics.total_packets++;
    fw_statistics.total_bytes += skb->len;
    
    /* Find chain */
    fw_chain_t *ch = NULL;
    for (u32 i = 0; i < FIREWALL_MAX_TABLES; i++) {
        fw_chain_t *c = fw_tables[i].chains;
        while (c) {
            if (c->id == chain) {
                ch = c;
                break;
            }
            c = c->next;
        }
    }
    
    if (!ch) {
        spin_unlock(&fw_lock);
        return FW_ACTION_ACCEPT;
    }
    
    ch->packets_processed++;
    ch->bytes_processed += skb->len;
    
    /* Evaluate rules */
    fw_rule_t *rule = ch->rules;
    while (rule) {
        fw_statistics.rules_evaluated++;
        
        /* Simple match - TODO: Full matching */
        bool match = true;
        
        if (match) {
            rule->packets_matched++;
            rule->bytes_matched += skb->len;
            
            /* Log if enabled */
            if (rule->log) {
                fw_log_rule_hit(rule, skb);
            }
            
            spin_unlock(&fw_lock);
            
            /* Apply action */
            switch (rule->action) {
            case FW_ACTION_ACCEPT:
                fw_statistics.accepted_packets++;
                fw_statistics.accepted_bytes += skb->len;
                return FW_ACTION_ACCEPT;
                
            case FW_ACTION_DROP:
                fw_statistics.dropped_packets++;
                fw_statistics.dropped_bytes += skb->len;
                return FW_ACTION_DROP;
                
            case FW_ACTION_REJECT:
                fw_statistics.rejected_packets++;
                return FW_ACTION_REJECT;
                
            case FW_ACTION_REDIRECT:
                return fw_nat_redirect(skb, &rule->redirect_addr, rule->redirect_port);
                
            case FW_ACTION_MASQUERADE:
                fw_statistics.nat_translations++;
                return fw_nat_masquerade(skb);
            }
        }
        
        rule = rule->next;
    }
    
    /* No match - apply default policy */
    u8 policy = ch->default_policy;
    spin_unlock(&fw_lock);
    
    return policy;
}

/**
 * fw_check_packet - Quick packet check
 */
int fw_check_packet(struct sk_buff *skb)
{
    return fw_process_packet(skb, FW_CHAIN_INPUT);
}

/*===========================================================================*/
/*                         CONNECTION TRACKING                               */
/*===========================================================================*/

/**
 * fw_conntrack_get - Get or create connection tracking entry
 */
fw_conntrack_t *fw_conntrack_get(struct sk_buff *skb)
{
    if (!skb) return NULL;
    
    /* Search existing connections */
    fw_conntrack_t *ct = fw_conntrack_table;
    while (ct) {
        /* TODO: Match packet to connection */
        ct = ct->next;
    }
    
    /* Create new connection */
    if (fw_conntrack_create(skb) == 0) {
        return fw_conntrack_table;
    }
    
    return NULL;
}

/**
 * fw_conntrack_create - Create connection tracking entry
 */
int fw_conntrack_create(struct sk_buff *skb)
{
    if (!skb) return -1;
    
    fw_conntrack_t *ct = kmalloc(sizeof(fw_conntrack_t));
    if (!ct) return -ENOMEM;
    
    ct->id = ++fw_conntrack_counter;
    ct->state = FW_STATE_NEW;
    ct->created = get_time_ms();
    ct->last_seen = ct->created;
    ct->timeout = 300000;  /* 5 minutes */
    ct->packets = 1;
    ct->bytes = skb->len;
    ct->related = NULL;
    
    /* Add to table */
    ct->next = fw_conntrack_table;
    fw_conntrack_table = ct;
    
    fw_statistics.conntrack_entries++;
    
    return 0;
}

/**
 * fw_conntrack_cleanup - Clean up connection tracking table
 */
void fw_conntrack_cleanup(void)
{
    spin_lock(&fw_lock);
    
    fw_conntrack_t *ct = fw_conntrack_table;
    while (ct) {
        fw_conntrack_t *next = ct->next;
        kfree(ct);
        ct = next;
    }
    
    fw_conntrack_table = NULL;
    fw_statistics.conntrack_entries = 0;
    
    spin_unlock(&fw_lock);
}

/*===========================================================================*/
/*                         NAT OPERATIONS                                    */
/*===========================================================================*/

/**
 * fw_nat_masquerade - Apply masquerade NAT
 */
int fw_nat_masquerade(struct sk_buff *skb)
{
    if (!skb) return -1;
    
    /* TODO: Implement masquerade NAT */
    printk("[NAT] Masquerade (TODO)\n");
    
    return 0;
}

/**
 * fw_nat_redirect - Apply redirect NAT
 */
int fw_nat_redirect(struct sk_buff *skb, fw_addr_t *addr, u16 port)
{
    if (!skb || !addr) return -1;
    
    /* TODO: Implement redirect NAT */
    printk("[NAT] Redirect to %p:%u (TODO)\n", addr, port);
    
    return 0;
}

/*===========================================================================*/
/*                         LOGGING                                           */
/*===========================================================================*/

/**
 * fw_log_packet - Log packet
 */
void fw_log_packet(struct sk_buff *skb, u8 level, const char *prefix)
{
    if (!skb || !prefix) return;
    
    const char *level_str[] = {
        "EMERG", "ALERT", "CRIT", "ERR", "WARN", "NOTICE", "INFO", "DEBUG"
    };
    
    printk("[FW-%s] %s: len=%u\n", level_str[level], prefix, skb->len);
}

/**
 * fw_log_rule_hit - Log rule match
 */
void fw_log_rule_hit(fw_rule_t *rule, struct sk_buff *skb)
{
    if (!rule || !skb) return;
    
    printk("[FW-RULE] %s matched (id=%u)\n", rule->name, rule->id);
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

/**
 * fw_get_stats - Get firewall statistics
 */
void fw_get_stats(fw_stats_t *stats)
{
    if (!stats) return;
    
    spin_lock(&fw_lock);
    memcpy(stats, &fw_statistics, sizeof(fw_stats_t));
    spin_unlock(&fw_lock);
}

/**
 * fw_dump_rules - Dump all firewall rules
 */
void fw_dump_rules(void)
{
    printk("\n=== Firewall Rules ===\n");
    
    spin_lock(&fw_lock);
    
    for (u32 i = 0; i < FIREWALL_MAX_TABLES; i++) {
        if (fw_tables[i].id == 0) continue;
        
        printk("\nTable: %s\n", fw_tables[i].name);
        
        fw_chain_t *ch = fw_tables[i].chains;
        while (ch) {
            printk("  Chain: %s (policy=%d, rules=%u)\n", 
                   ch->name, ch->default_policy, ch->rule_count);
            
            fw_rule_t *rule = ch->rules;
            while (rule) {
                printk("    Rule %u: %s (action=%d, pkts=%lu)\n",
                       rule->id, rule->name, rule->action, 
                       (unsigned long)rule->packets_matched);
                rule = rule->next;
            }
            
            ch = ch->next;
        }
    }
    
    spin_unlock(&fw_lock);
    printk("\n");
}

/**
 * fw_dump_conntrack - Dump connection tracking table
 */
void fw_dump_conntrack(void)
{
    printk("\n=== Connection Tracking ===\n");
    
    spin_lock(&fw_lock);
    
    fw_conntrack_t *ct = fw_conntrack_table;
    u32 count = 0;
    
    while (ct && count < 100) {
        printk("  [%u] state=%d pkts=%lu bytes=%lu\n",
               ct->id, ct->state,
               (unsigned long)ct->packets,
               (unsigned long)ct->bytes);
        ct = ct->next;
        count++;
    }
    
    spin_unlock(&fw_lock);
    printk("Total: %lu entries\n\n", (unsigned long)fw_statistics.conntrack_entries);
}

/*===========================================================================*/
/*                         PREDEFINED RULES                                  */
/*===========================================================================*/

/**
 * fw_allow_established - Allow established connections
 */
int fw_allow_established(void)
{
    fw_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    
    strcpy(rule.name, "allow-established");
    rule.table = 1;  /* filter */
    rule.chain = 1;  /* INPUT */
    rule.state_mask = FW_STATE_ESTABLISHED | FW_STATE_RELATED;
    rule.action = FW_ACTION_ACCEPT;
    
    return fw_rule_add(&rule);
}

/**
 * fw_allow_loopback - Allow loopback traffic
 */
int fw_allow_loopback(void)
{
    fw_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    
    strcpy(rule.name, "allow-loopback");
    rule.table = 1;
    rule.chain = 1;
    rule.interface_in = 1;  /* lo */
    rule.action = FW_ACTION_ACCEPT;
    
    return fw_rule_add(&rule);
}

/**
 * fw_allow_icmp - Allow ICMP
 */
int fw_allow_icmp(void)
{
    fw_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    
    strcpy(rule.name, "allow-icmp");
    rule.table = 1;
    rule.chain = 1;
    rule.protocol = FW_PROTO_ICMP;
    rule.action = FW_ACTION_ACCEPT;
    
    return fw_rule_add(&rule);
}

/**
 * fw_allow_port - Allow specific port
 */
int fw_allow_port(u8 proto, u16 port)
{
    fw_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    
    snprintf(rule.name, sizeof(rule.name), "allow-port-%u", port);
    rule.table = 1;
    rule.chain = 1;
    rule.protocol = proto;
    rule.dst_port.start = port;
    rule.dst_port.end = port;
    rule.action = FW_ACTION_ACCEPT;
    
    return fw_rule_add(&rule);
}

/**
 * fw_block_all - Block all traffic
 */
int fw_block_all(void)
{
    fw_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    
    strcpy(rule.name, "block-all");
    rule.table = 1;
    rule.chain = 1;
    rule.action = FW_ACTION_DROP;
    
    return fw_rule_add(&rule);
}

/**
 * fw_profile_reset - Reset to default profile
 */
int fw_profile_reset(void)
{
    printk("[FIREWALL] Resetting to default profile...\n");
    
    /* Flush all rules */
    fw_rule_flush("filter", "INPUT");
    fw_rule_flush("filter", "OUTPUT");
    fw_rule_flush("filter", "FORWARD");
    
    /* Add basic security */
    fw_allow_loopback();
    fw_allow_established();
    fw_allow_icmp();
    
    return 0;
}
