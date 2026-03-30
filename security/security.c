/*
 * NEXUS OS - Security Subsystem
 * security/security.c
 */

#include "../include/kernel.h"

extern void security_module_init(void);

/**
 * security_core_init - Initialize the security subsystem core
 */
void security_core_init(void)
{
    pr_info("Initializing Security Subsystem...\n");
    
    security_module_init();
    
    pr_info("Security Subsystem initialized.\n");
}

/* Stubs for missing security functions */
s32 security_crypto_init(void) { return 0; }
s32 security_auth_init(void) { return 0; }
s32 security_sandbox_init(void) { return 0; }
s32 security_tpm_init(void) { return 0; }
