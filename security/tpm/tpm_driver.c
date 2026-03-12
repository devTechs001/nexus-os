/*
 * NEXUS OS - Security Framework
 * security/tpm/tpm_driver.c
 *
 * TPM (Trusted Platform Module) Hardware Driver
 * TPM 1.2 and TPM 2.0 support
 */

#include "../security.h"

/*===========================================================================*/
/*                         TPM CONFIGURATION                                 */
/*===========================================================================*/

#define TPM_DEBUG               0
#define TPM_MAX_DEVICES         4
#define TPM_MAX_COMMAND_SIZE    4096
#define TPM_MAX_RESPONSE_SIZE   4096
#define TPM_TIMEOUT_MS          5000
#define TPM_RETRY_COUNT         3

/* TPM Versions */
#define TPM_VERSION_UNKNOWN     0
#define TPM_VERSION_1_2         1
#define TPM_VERSION_2_0         2

/* TPM Device Types */
#define TPM_DEV_TYPE_NONE       0
#define TPM_DEV_TYPE_LPC        1
#define TPM_DEV_TYPE_ACPI       2
#define TPM_DEV_TYPE_MMIO       3
#define TPM_DEV_TYPE_SPI        4
#define TPM_DEV_TYPE_I2C        5

/* TPM Command Tags */
#define TPM_TAG_RQU_COMMAND     0x00C1
#define TPM_TAG_RQU_AUTH1_CMD   0x00C2
#define TPM_TAG_RQU_AUTH2_CMD   0x00C3

/* TPM Response Tags */
#define TPM_TAG_RSP_COMMAND     0x00C4
#define TPM_TAG_RSP_AUTH1_CMD   0x00C5
#define TPM_TAG_RSP_AUTH2_CMD   0x00C6

/* TPM 2.0 Command Codes */
#define TPM2_CC_NV_UndefineSpaceSpecial   0x0000011F
#define TPM2_CC_EvictControl              0x00000120
#define TPM2_CC_HierarchyControl          0x00000121
#define TPM2_CC_NV_UndefineSpace          0x00000122
#define TPM2_CC_ChangeEPS                 0x00000124
#define TPM2_CC_ChangePPS                 0x00000125
#define TPM2_CC_Clear                     0x00000126
#define TPM2_CC_ClearControl              0x00000127
#define TPM2_CC_ClockSet                  0x00000128
#define TPM2_CC_HierarchyChangeAuth       0x00000129
#define TPM2_CC_NV_DefineSpace            0x0000012A
#define TPM2_CC_PCR_Allocate              0x0000012B
#define TPM2_CC_PCR_SetAuthPolicy         0x0000012C
#define TPM2_CC_PP_Commands               0x0000012D
#define TPM2_CC_SetPrimaryPolicy          0x0000012E
#define TPM2_CC_FieldUpgradeStart         0x0000012F
#define TPM2_CC_ClockRateAdjust           0x00000130
#define TPM2_CC_CreatePrimary             0x00000131
#define TPM2_CC_NV_GlobalWriteLock        0x00000132
#define TPM2_CC_GetCommandAuditDigest     0x00000133
#define TPM2_CC_NV_Increment              0x00000134
#define TPM2_CC_NV_SetBits                0x00000135
#define TPM2_CC_NV_Extend                 0x00000136
#define TPM2_CC_NV_Write                  0x00000137
#define TPM2_CC_NV_WriteLock              0x00000138
#define TPM2_CC_DictionaryAttackLockReset 0x00000139
#define TPM2_CC_DictionaryAttackParameters 0x0000013A
#define TPM2_CC_NV_ChangeAuth             0x0000013B
#define TPM2_CC_PCR_Event                 0x0000013C
#define TPM2_CC_PCR_Reset                 0x0000013D
#define TPM2_CC_SequenceComplete          0x0000013E
#define TPM2_CC_SetAlgorithmSet           0x0000013F
#define TPM2_CC_SetCommandCodeAuditStatus 0x00000140
#define TPM2_CC_FieldUpgradeData          0x00000141
#define TPM2_CC_IncrementalSelfTest       0x00000142
#define TPM2_CC_SelfTest                  0x00000143
#define TPM2_CC_Startup                   0x00000144
#define TPM2_CC_Shutdown                  0x00000145
#define TPM2_CC_StirRandom                0x00000146
#define TPM2_CC_ActivateCredential        0x00000147
#define TPM2_CC_Certify                   0x00000148
#define TPM2_CC_PolicyNV                  0x00000149
#define TPM2_CC_CertifyCreation           0x0000014A
#define TPM2_CC_Duplicate                 0x0000014B
#define TPM2_CC_GetTime                   0x0000014C
#define TPM2_CC_GetSessionAuditDigest     0x0000014D
#define TPM2_CC_NV_Read                   0x0000014E
#define TPM2_CC_NV_ReadLock               0x0000014F
#define TPM2_CC_ObjectChangeAuth          0x00000150
#define TPM2_CC_PolicySecret              0x00000151
#define TPM2_CC_Rewrap                    0x00000152
#define TPM2_CC_Create                    0x00000153
#define TPM2_CC_ECDH_ZGen                 0x00000154
#define TPM2_CC_HMAC                      0x00000155
#define TPM2_CC_Import                    0x00000156
#define TPM2_CC_Load                      0x00000157
#define TPM2_CC_Quote                     0x00000158
#define TPM2_CC_RSA_Decrypt               0x00000159
#define TPM2_CC_HMAC_Start                0x0000015B
#define TPM2_CC_SequenceUpdate            0x0000015C
#define TPM2_CC_Sign                      0x0000015D
#define TPM2_CC_Unseal                    0x0000015E
#define TPM2_CC_PolicySigned              0x00000160
#define TPM2_CC_ContextLoad               0x00000161
#define TPM2_CC_ContextSave               0x00000162
#define TPM2_CC_ECDH_KeyGen               0x00000163
#define TPM2_CC_EncryptDecrypt            0x00000164
#define TPM2_CC_FlushContext              0x00000165
#define TPM2_CC_LoadExternal              0x00000167
#define TPM2_CC_MakeCredential            0x00000168
#define TPM2_CC_NV_ReadPublic             0x00000169
#define TPM2_CC_PolicyAuthorize           0x0000016A
#define TPM2_CC_PolicyAuthValue           0x0000016B
#define TPM2_CC_PolicyCommandCode         0x0000016C
#define TPM2_CC_PolicyCounterTimer        0x0000016D
#define TPM2_CC_PolicyCpHash              0x0000016E
#define TPM2_CC_PolicyLocality            0x0000016F
#define TPM2_CC_PolicyNameHash            0x00000170
#define TPM2_CC_PolicyOR                  0x00000171
#define TPM2_CC_PolicyTicket              0x00000172
#define TPM2_CC_ReadPublic                0x00000173
#define TPM2_CC_RSA_Encrypt               0x00000174
#define TPM2_CC_StartAuthSession          0x00000176
#define TPM2_CC_VerifySignature           0x00000177
#define TPM2_CC_ECC_Parameters            0x00000178
#define TPM2_CC_FirmwareRead              0x00000179
#define TPM2_CC_GetCapability             0x0000017A
#define TPM2_CC_GetRandom                 0x0000017B
#define TPM2_CC_GetTestResult             0x0000017C
#define TPM2_CC_Hash                      0x0000017D
#define TPM2_CC_PCR_Read                  0x0000017E
#define TPM2_CC_PolicyPCR                 0x0000017F
#define TPM2_CC_PolicyRestart             0x00000180
#define TPM2_CC_ReadClock                 0x00000181
#define TPM2_CC_PCR_Extend                0x00000182
#define TPM2_CC_PCR_SetAuthValue          0x00000183
#define TPM2_CC_NV_Certify                0x00000184
#define TPM2_CC_EventSequenceComplete     0x00000185
#define TPM2_CC_HashSequenceStart         0x00000186
#define TPM2_CC_PolicyPhysicalPresence    0x00000187
#define TPM2_CC_PolicyDuplicationSelect   0x00000188
#define TPM2_CC_PolicyGetDigest           0x00000189
#define TPM2_CC_TestParms                 0x0000018A
#define TPM2_CC_Commit                    0x0000018B
#define TPM2_CC_PolicyPassword            0x0000018C
#define TPM2_CC_ZGen_2Phase               0x0000018D
#define TPM2_CC_EC_Ephemeral              0x0000018E
#define TPM2_CC_PolicyNvWritten           0x0000018F
#define TPM2_CC_PolicyTemplate            0x00000190
#define TPM2_CC_CreateLoaded              0x00000191
#define TPM2_CC_PolicyAuthorizeNV         0x00000192
#define TPM2_CC_EncryptDecrypt2           0x00000193
#define TPM2_CC_AC_GetCapability          0x00000194
#define TPM2_CC_AC_Send                   0x00000195
#define TPM2_CC_PolicyAC_SendSelect       0x00000196
#define TPM2_CC_CertifyX509               0x00000197
#define TPM2_CC_ActivateCredentialTPM20   0x00000198
#define TPM2_CC_PolicyCapability          0x00000199
#define TPM2_CC_PolicyParametersHash      0x0000019A

/* TPM Return Codes */
#define TPM_RC_SUCCESS            0x000
#define TPM_RC_BAD_TAG            0x030
#define TPM_RC_AUTHFAIL           0x024
#define TPM_RC_BADINDEX           0x025
#define TPM_RC_BADPARAMETER       0x026
#define TPM_RC_COMMAND_CODE       0x027
#define TPM_RC_DISABLED           0x028
#define TPM_RC_FAIL               0x029
#define TPM_RC_INACTIVE           0x033
#define TPM_RC_INSTALL_DISABLED   0x034
#define TPM_RC_INVALID_KEYHANDLE  0x035
#define TPM_RC_KEYNOTFOUND        0x036
#define TPM_RC_NEEDS_TEST         0x038
#define TPM_RC_NOOPERATOR         0x039
#define TPM_RC_NOTREADY           0x03A
#define TPM_RC_NOT_SUPPORTED      0x03B
#define TPM_RC_OWNER_SET          0x03C
#define TPM_RC_RESOURCES          0x03D
#define TPM_RC_SHORTRANDOM        0x03E
#define TPM_RC_SIZE               0x03F
#define TPM_RC_WRONGPCRVAL        0x040
#define TPM_RC_BAD_HANDLE         0x08B
#define TPM_RC_BAD_VALUE          0x08C
#define TPM_RC_HASH_TYPE          0x08D
#define TPM_RC_RANGE              0x091

/* Magic Numbers */
#define TPM_DEVICE_MAGIC          0x54504D44455601ULL  /* TPMDEV01 */

/*===========================================================================*/
/*                         TPM DATA STRUCTURES                               */
/*===========================================================================*/

/**
 * tpm_device_t - TPM device structure
 */
typedef struct {
    u64 magic;                  /* Magic number */
    spinlock_t lock;            /* Protection lock */
    u32 id;                     /* Device ID */
    u32 type;                   /* Device type */
    u32 version;                /* TPM version */
    bool present;               /* Device present */
    bool active;                /* Device active */
    bool owned;                 /* TPM has owner */
    u32 base_addr;              /* Base address */
    u32 io_region;              /* I/O region */
    u32 irq;                    /* IRQ number */
    u64 caps;                   /* Capabilities */
    char manufacturer[32];      /* Manufacturer ID */
    u32 firmware_major;         /* Firmware major version */
    u32 firmware_minor;         /* Firmware minor version */
    u8 ek_public[512];          /* Endorsement key public */
    u32 ek_size;                /* EK public size */
    u64 total_commands;         /* Total commands sent */
    u64 total_errors;           /* Total errors */
} tpm_device_t;

/**
 * tpm_pcr_t - PCR (Platform Configuration Register) value
 */
typedef struct {
    u32 pcr_index;              /* PCR index */
    u8 digest[SHA256_DIGEST_SIZE];  /* PCR value */
    u32 digest_size;            /* Digest size */
    u64 extended_count;         /* Extension count */
} tpm_pcr_t;

/**
 * tpm_key_t - TPM key handle
 */
typedef struct {
    u32 handle;                 /* Key handle */
    u32 algorithm;              /* Key algorithm */
    u32 size;                   /* Key size in bits */
    u8 public_key[512];         /* Public key */
    u32 public_size;            /* Public key size */
    char name[64];              /* Key name */
    u32 attributes;             /* Key attributes */
} tpm_key_t;

/**
 * tpm_context_t - TPM operation context
 */
typedef struct {
    tpm_device_t *device;       /* TPM device */
    u32 session_handle;         /* Session handle */
    u8 session_nonce[32];       /* Session nonce */
    u32 sequence;               /* Command sequence */
} tpm_context_t;

/**
 * tpm_manager_t - TPM manager state
 */
typedef struct {
    spinlock_t lock;            /* Manager lock */
    u32 state;                  /* Manager state */
    tpm_device_t *devices[TPM_MAX_DEVICES];
    u32 device_count;           /* Active devices */
    tpm_device_t *primary;      /* Primary TPM */
    u32 next_id;                /* Next device ID */
    u64 total_operations;       /* Total operations */
} tpm_manager_t;

/*===========================================================================*/
/*                         TPM GLOBAL DATA                                   */
/*===========================================================================*/

static tpm_manager_t g_tpm_manager = {
    .lock = {
        .lock = SPINLOCK_UNLOCKED,
        .magic = SPINLOCK_MAGIC,
        .name = "tpm_manager",
    },
    .state = 0,
    .device_count = 0,
    .primary = NULL,
    .next_id = 1,
};

/*===========================================================================*/
/*                         TPM LOW-LEVEL I/O                                 */
/*===========================================================================*/

/**
 * tpm_read_reg - Read TPM register
 * @dev: TPM device
 * @reg: Register offset
 *
 * Returns: Register value
 */
static u32 tpm_read_reg(tpm_device_t *dev, u32 reg)
{
    u32 value;

    if (!dev || !dev->present) {
        return 0;
    }

    /* In real implementation, would use appropriate I/O method */
    /* based on device type (LPC, MMIO, SPI, etc.) */

    switch (dev->type) {
        case TPM_DEV_TYPE_MMIO:
            /* Memory-mapped I/O */
            value = *(volatile u32 *)(dev->base_addr + reg);
            break;

        case TPM_DEV_TYPE_LPC:
        case TPM_DEV_TYPE_ACPI:
            /* Port I/O */
            /* value = inl(dev->io_region + reg); */
            value = 0;
            break;

        default:
            value = 0;
            break;
    }

    return value;
}

/**
 * tpm_write_reg - Write TPM register
 * @dev: TPM device
 * @reg: Register offset
 * @value: Value to write
 */
static void tpm_write_reg(tpm_device_t *dev, u32 reg, u32 value)
{
    if (!dev || !dev->present) {
        return;
    }

    switch (dev->type) {
        case TPM_DEV_TYPE_MMIO:
            *(volatile u32 *)(dev->base_addr + reg) = value;
            break;

        case TPM_DEV_TYPE_LPC:
        case TPM_DEV_TYPE_ACPI:
            /* outl(value, dev->io_region + reg); */
            break;

        default:
            break;
    }
}

/**
 * tpm_wait_ready - Wait for TPM to be ready
 * @dev: TPM device
 * @timeout_ms: Timeout in milliseconds
 *
 * Returns: OK on success, ETIMEDOUT on timeout
 */
static s32 tpm_wait_ready(tpm_device_t *dev, u32 timeout_ms)
{
    u64 start_time = get_time_ms();
    u32 status;

    while ((get_time_ms() - start_time) < timeout_ms) {
        status = tpm_read_reg(dev, 0x18);  /* TPM_STS register */

        if (status & 0x40) {  /* TPM_STS_COMMAND_READY */
            return OK;
        }

        delay_ms(1);
    }

    return ETIMEDOUT;
}

/**
 * tpm_wait_data_avail - Wait for data to be available
 * @dev: TPM device
 * @timeout_ms: Timeout in milliseconds
 *
 * Returns: OK on success, ETIMEDOUT on timeout
 */
static s32 tpm_wait_data_avail(tpm_device_t *dev, u32 timeout_ms)
{
    u64 start_time = get_time_ms();
    u32 status;

    while ((get_time_ms() - start_time) < timeout_ms) {
        status = tpm_read_reg(dev, 0x18);  /* TPM_STS register */

        if (status & 0x10) {  /* TPM_STS_DATA_AVAILABLE */
            return OK;
        }

        delay_ms(1);
    }

    return ETIMEDOUT;
}

/*===========================================================================*/
/*                         TPM INITIALIZATION                                */
/*===========================================================================*/

/**
 * tpm_init - Initialize TPM subsystem
 *
 * Initializes the TPM subsystem and probes for devices.
 *
 * Returns: OK on success, error code on failure
 */
s32 tpm_init(void)
{
    pr_info("TPM: Initializing TPM subsystem...\n");

    spin_lock_init_named(&g_tpm_manager.lock, "tpm_manager");
    g_tpm_manager.state = 1;

    /* Initialize device table */
    for (u32 i = 0; i < TPM_MAX_DEVICES; i++) {
        g_tpm_manager.devices[i] = NULL;
    }

    g_tpm_manager.state = 2;

    pr_info("TPM: Subsystem initialized\n");

    return OK;
}

/**
 * tpm_probe - Probe for TPM devices
 *
 * Probes for available TPM devices in the system.
 *
 * Returns: Number of devices found, or error code
 */
s32 tpm_probe(void)
{
    tpm_device_t *dev;
    u32 device_count = 0;
    u32 i;

    pr_info("TPM: Probing for TPM devices...\n");

    /* Try to detect TPM at standard locations */
    /* In real implementation, would check ACPI tables, device tree, etc. */

    /* Simulate finding a TPM device */
    for (i = 0; i < TPM_MAX_DEVICES; i++) {
        if (g_tpm_manager.devices[i] == NULL) {
            dev = kzalloc(sizeof(tpm_device_t));
            if (!dev) {
                break;
            }

            dev->magic = TPM_DEVICE_MAGIC;
            spin_lock_init_named(&dev->lock, "tpm_device");
            dev->id = g_tpm_manager.next_id++;
            dev->type = TPM_DEV_TYPE_MMIO;  /* Simulated */
            dev->version = TPM_VERSION_2_0;
            dev->present = true;
            dev->active = false;
            dev->base_addr = 0xFED40000;  /* Standard TPM MMIO address */

            strncpy(dev->manufacturer, "NEXUS", sizeof(dev->manufacturer) - 1);
            dev->firmware_major = 1;
            dev->firmware_minor = 0;

            g_tpm_manager.devices[i] = dev;
            g_tpm_manager.device_count++;
            device_count++;

            /* Set as primary if first device */
            if (!g_tpm_manager.primary) {
                g_tpm_manager.primary = dev;
            }

            pr_info("TPM: Found device %u at 0x%08X (TPM 2.0)\n",
                    dev->id, dev->base_addr);

            break;  /* Only simulate one device */
        }
    }

    if (device_count > 0) {
        pr_info("TPM: Found %u device(s)\n", device_count);
    } else {
        pr_info("TPM: No TPM devices found\n");
    }

    return device_count;
}

/**
 * tpm_shutdown - Shutdown TPM subsystem
 *
 * Shuts down the TPM subsystem and releases resources.
 */
void tpm_shutdown(void)
{
    u32 i;

    pr_info("TPM: Shutting down...\n");

    spin_lock(&g_tpm_manager.lock);

    /* Shutdown all devices */
    for (i = 0; i < TPM_MAX_DEVICES; i++) {
        if (g_tpm_manager.devices[i]) {
            kfree(g_tpm_manager.devices[i]);
            g_tpm_manager.devices[i] = NULL;
        }
    }

    g_tpm_manager.device_count = 0;
    g_tpm_manager.primary = NULL;
    g_tpm_manager.state = 0;

    spin_unlock(&g_tpm_manager.lock);

    pr_info("TPM: Shutdown complete\n");
}

/*===========================================================================*/
/*                         TPM COMMAND INTERFACE                             */
/*===========================================================================*/

/**
 * tpm_transmit - Transmit command to TPM
 * @dev: TPM device
 * @command: Command buffer
 * @command_size: Command size
 * @response: Response buffer
 * @response_size: Response buffer size
 *
 * Sends a command to the TPM and receives the response.
 *
 * Returns: Response size on success, error code on failure
 */
static s32 tpm_transmit(tpm_device_t *dev, const u8 *command, u32 command_size,
                        u8 *response, u32 response_size)
{
    u32 i;
    s32 ret;

    if (!dev || !dev->present || !command || !response) {
        return EINVAL;
    }

    if (command_size > TPM_MAX_COMMAND_SIZE) {
        return E2BIG;
    }

    spin_lock(&dev->lock);

    dev->total_commands++;

    /* Wait for TPM to be ready */
    ret = tpm_wait_ready(dev, TPM_TIMEOUT_MS);
    if (ret != OK) {
        dev->total_errors++;
        spin_unlock(&dev->lock);
        return ret;
    }

    /* Write command to TPM FIFO */
    /* In real implementation, would write to TPM data register */
    for (i = 0; i < command_size; i++) {
        /* tpm_write_reg(dev, 0x24, command[i]); */  /* TPM_DATA register */
    }

    /* Wait for response */
    ret = tpm_wait_data_avail(dev, TPM_TIMEOUT_MS);
    if (ret != OK) {
        dev->total_errors++;
        spin_unlock(&dev->lock);
        return ret;
    }

    /* Read response from TPM FIFO */
    /* In real implementation, would read from TPM data register */
    for (i = 0; i < response_size && i < command_size; i++) {
        /* response[i] = tpm_read_reg(dev, 0x24) & 0xFF; */
        response[i] = command[i];  /* Simulated echo */
    }

    spin_unlock(&dev->lock);

    return i;
}

/**
 * tpm_send_command - Send TPM command
 * @ctx: TPM context
 * @command_code: Command code
 * @input: Input parameters
 * @input_size: Input size
 * @output: Output buffer
 * @output_size: Output buffer size
 *
 * Sends a TPM command and returns the result.
 *
 * Returns: OK on success, error code on failure
 */
static s32 tpm_send_command(tpm_context_t *ctx, u32 command_code,
                            const u8 *input, u32 input_size,
                            u8 *output, u32 *output_size)
{
    u8 command[TPM_MAX_COMMAND_SIZE];
    u8 response[TPM_MAX_RESPONSE_SIZE];
    u32 command_size;
    s32 ret;

    if (!ctx || !ctx->device) {
        return EINVAL;
    }

    /* Build TPM 2.0 command header */
    command[0] = 0x80;  /* TPM_ST_NO_SESSIONS */
    command[1] = 0x01;
    command[2] = 0x00;
    command[3] = 0x00;  /* Size filled in below */
    command[4] = (command_code >> 24) & 0xFF;
    command[5] = (command_code >> 16) & 0xFF;
    command[6] = (command_code >> 8) & 0xFF;
    command[7] = command_code & 0xFF;

    if (input && input_size > 0) {
        memcpy(command + 8, input, input_size);
    }

    command_size = 10 + input_size;
    command[3] = (command_size >> 24) & 0xFF;
    command[2] = (command_size >> 16) & 0xFF;
    command[1] = (command_size >> 8) & 0xFF;
    command[0] = command_size & 0xFF;

    /* Transmit command */
    ret = tpm_transmit(ctx->device, command, command_size,
                       response, sizeof(response));
    if (ret < 0) {
        return ret;
    }

    /* Parse response */
    if (ret < 10) {
        return EIO;
    }

    /* Check response code */
    u32 rc = (response[6] << 16) | (response[7] << 8) | response[8];
    if ((rc & 0x100) != 0) {  /* Error bit set */
        return EIO;
    }

    /* Copy response data */
    if (output && output_size) {
        u32 copy_size = ret - 10;
        if (copy_size > *output_size) {
            copy_size = *output_size;
        }
        memcpy(output, response + 10, copy_size);
        *output_size = copy_size;
    }

    return OK;
}

/*===========================================================================*/
/*                         TPM OPERATIONS                                    */
/*===========================================================================*/

/**
 * tpm_startup - Initialize TPM
 * @dev: TPM device
 * @startup_type: Startup type (CLEAR/STATE)
 *
 * Initializes the TPM after power-on.
 *
 * Returns: OK on success, error code on failure
 */
s32 tpm_startup(tpm_device_t *dev, u16 startup_type)
{
    tpm_context_t ctx;
    u8 input[2];
    u32 output_size = 0;
    s32 ret;

    if (!dev || !dev->present) {
        return ENODEV;
    }

    ctx.device = dev;
    ctx.session_handle = 0;

    /* Build startup input */
    input[0] = (startup_type >> 8) & 0xFF;
    input[1] = startup_type & 0xFF;

    ret = tpm_send_command(&ctx, TPM2_CC_Startup, input, 2, NULL, &output_size);

    if (ret == OK) {
        dev->active = true;
        pr_info("TPM: Device %u started\n", dev->id);
    }

    return ret;
}

/**
 * tpm_shutdown_tpm - Shutdown TPM
 * @dev: TPM device
 * @shutdown_type: Shutdown type
 *
 * Shuts down the TPM gracefully.
 *
 * Returns: OK on success, error code on failure
 */
s32 tpm_shutdown_tpm(tpm_device_t *dev, u16 shutdown_type)
{
    tpm_context_t ctx;
    u8 input[2];
    u32 output_size = 0;

    if (!dev || !dev->present) {
        return ENODEV;
    }

    ctx.device = dev;

    input[0] = (shutdown_type >> 8) & 0xFF;
    input[1] = shutdown_type & 0xFF;

    dev->active = false;

    return tpm_send_command(&ctx, TPM2_CC_Shutdown, input, 2, NULL, &output_size);
}

/**
 * tpm_get_random - Get random bytes from TPM
 * @dev: TPM device
 * @buffer: Output buffer
 * @size: Number of bytes requested
 *
 * Gets random bytes from the TPM's RNG.
 *
 * Returns: Number of bytes returned, or error code
 */
s32 tpm_get_random(tpm_device_t *dev, u8 *buffer, u32 size)
{
    tpm_context_t ctx;
    u8 input[4];
    u8 output[TPM_MAX_RESPONSE_SIZE];
    u32 output_size = sizeof(output);
    s32 ret;

    if (!dev || !dev->present || !buffer || size == 0) {
        return EINVAL;
    }

    ctx.device = dev;

    /* Request random bytes */
    input[0] = (size >> 24) & 0xFF;
    input[1] = (size >> 16) & 0xFF;
    input[2] = (size >> 8) & 0xFF;
    input[3] = size & 0xFF;

    ret = tpm_send_command(&ctx, TPM2_CC_GetRandom, input, 4, output, &output_size);
    if (ret != OK) {
        return ret;
    }

    /* Parse response */
    if (output_size < 2) {
        return EIO;
    }

    u32 random_size = (output[0] << 8) | output[1];
    if (random_size > size) {
        random_size = size;
    }

    memcpy(buffer, output + 2, random_size);

    return random_size;
}

/**
 * tpm_pcr_extend - Extend PCR value
 * @dev: TPM device
 * @pcr_index: PCR index
 * @digest: Digest to extend
 * @digest_size: Digest size
 *
 * Extends a PCR with a new digest value.
 *
 * Returns: OK on success, error code on failure
 */
s32 tpm_pcr_extend(tpm_device_t *dev, u32 pcr_index, const u8 *digest, u32 digest_size)
{
    tpm_context_t ctx;
    u8 input[64];
    u32 input_size;
    u32 output_size = 0;

    if (!dev || !dev->present || !digest) {
        return EINVAL;
    }

    if (digest_size > SHA256_DIGEST_SIZE) {
        return EINVAL;
    }

    ctx.device = dev;

    /* Build extend input */
    input[0] = (pcr_index >> 24) & 0xFF;
    input[1] = (pcr_index >> 16) & 0xFF;
    input[2] = (pcr_index >> 8) & 0xFF;
    input[3] = pcr_index & 0xFF;

    /* Hash algorithm (SHA256 = 0x0B) */
    input[4] = 0x00;
    input[5] = 0x0B;

    input[6] = (digest_size >> 24) & 0xFF;
    input[7] = (digest_size >> 16) & 0xFF;
    input[8] = (digest_size >> 8) & 0xFF;
    input[9] = digest_size & 0xFF;

    memcpy(input + 10, digest, digest_size);
    input_size = 10 + digest_size;

    return tpm_send_command(&ctx, TPM2_CC_PCR_Extend, input, input_size,
                            NULL, &output_size);
}

/**
 * tpm_pcr_read - Read PCR value
 * @dev: TPM device
 * @pcr_index: PCR index
 * @digest: Output digest buffer
 * @digest_size: Digest size
 *
 * Reads a PCR value.
 *
 * Returns: OK on success, error code on failure
 */
s32 tpm_pcr_read(tpm_device_t *dev, u32 pcr_index, u8 *digest, u32 *digest_size)
{
    tpm_context_t ctx;
    u8 input[4];
    u8 output[TPM_MAX_RESPONSE_SIZE];
    u32 output_size = sizeof(output);
    s32 ret;

    if (!dev || !dev->present || !digest || !digest_size) {
        return EINVAL;
    }

    ctx.device = dev;

    input[0] = (pcr_index >> 24) & 0xFF;
    input[1] = (pcr_index >> 16) & 0xFF;
    input[2] = (pcr_index >> 8) & 0xFF;
    input[3] = pcr_index & 0xFF;

    ret = tpm_send_command(&ctx, TPM2_CC_PCR_Read, input, 4, output, &output_size);
    if (ret != OK) {
        return ret;
    }

    /* Parse response */
    if (output_size < 6) {
        return EIO;
    }

    u32 digest_len = (output[4] << 24) | (output[5] << 16) |
                     (output[6] << 8) | output[7];

    if (digest_len > *digest_size) {
        digest_len = *digest_size;
    }

    memcpy(digest, output + 8, digest_len);
    *digest_size = digest_len;

    return OK;
}

/**
 * tpm_hash - Hash data using TPM
 * @dev: TPM device
 * @data: Data to hash
 * @data_size: Data size
 * @hash: Output hash
 * @hash_size: Hash size
 * @algorithm: Hash algorithm
 *
 * Hashes data using the TPM's hash engine.
 *
 * Returns: OK on success, error code on failure
 */
s32 tpm_hash(tpm_device_t *dev, const u8 *data, u32 data_size,
             u8 *hash, u32 *hash_size, u16 algorithm)
{
    tpm_context_t ctx;
    u8 input[TPM_MAX_COMMAND_SIZE];
    u32 input_size;
    u8 output[TPM_MAX_RESPONSE_SIZE];
    u32 output_size = sizeof(output);
    s32 ret;

    if (!dev || !dev->present || !data || !hash || !hash_size) {
        return EINVAL;
    }

    if (data_size > TPM_MAX_COMMAND_SIZE - 16) {
        return E2BIG;
    }

    ctx.device = dev;

    /* Build hash input */
    input[0] = (algorithm >> 8) & 0xFF;
    input[1] = algorithm & 0xFF;

    input[2] = 0x00;  /* Hierarchy (TPM_RH_NULL) */
    input[3] = 0x00;
    input[4] = 0x00;
    input[5] = 0x00;

    input[6] = (data_size >> 24) & 0xFF;
    input[7] = (data_size >> 16) & 0xFF;
    input[8] = (data_size >> 8) & 0xFF;
    input[9] = data_size & 0xFF;

    memcpy(input + 10, data, data_size);
    input_size = 10 + data_size;

    ret = tpm_send_command(&ctx, TPM2_CC_Hash, input, input_size,
                           output, &output_size);
    if (ret != OK) {
        return ret;
    }

    /* Parse response */
    if (output_size < 2) {
        return EIO;
    }

    u32 result_size = (output[0] << 8) | output[1];
    if (result_size > *hash_size) {
        result_size = *hash_size;
    }

    memcpy(hash, output + 2, result_size);
    *hash_size = result_size;

    return OK;
}

/**
 * tpm_get_capability - Get TPM capabilities
 * @dev: TPM device
 * @capability: Capability area
 * @property: Property ID
 * @count: Number of properties
 * @output: Output buffer
 * @output_size: Output buffer size
 *
 * Queries TPM capabilities.
 *
 * Returns: OK on success, error code on failure
 */
s32 tpm_get_capability(tpm_device_t *dev, u32 capability, u32 property,
                       u32 count, u8 *output, u32 *output_size)
{
    tpm_context_t ctx;
    u8 input[12];
    s32 ret;

    if (!dev || !dev->present || !output || !output_size) {
        return EINVAL;
    }

    ctx.device = dev;

    input[0] = (capability >> 24) & 0xFF;
    input[1] = (capability >> 16) & 0xFF;
    input[2] = (capability >> 8) & 0xFF;
    input[3] = capability & 0xFF;

    input[4] = (property >> 24) & 0xFF;
    input[5] = (property >> 16) & 0xFF;
    input[6] = (property >> 8) & 0xFF;
    input[7] = property & 0xFF;

    input[8] = (count >> 24) & 0xFF;
    input[9] = (count >> 16) & 0xFF;
    input[10] = (count >> 8) & 0xFF;
    input[11] = count & 0xFF;

    return tpm_send_command(&ctx, TPM2_CC_GetCapability, input, 12,
                            output, output_size);
}

/*===========================================================================*/
/*                         TPM STATISTICS                                    */
/*===========================================================================*/

/**
 * tpm_stats - Print TPM statistics
 */
void tpm_stats(void)
{
    u32 i;
    tpm_device_t *dev;

    printk("\n=== TPM Statistics ===\n");
    printk("State: %u\n", g_tpm_manager.state);
    printk("Active Devices: %u\n", g_tpm_manager.device_count);
    printk("Total Operations: %llu\n",
           (unsigned long long)g_tpm_manager.total_operations);

    printk("\n=== TPM Devices ===\n");
    for (i = 0; i < TPM_MAX_DEVICES; i++) {
        dev = g_tpm_manager.devices[i];
        if (dev && dev->magic == TPM_DEVICE_MAGIC) {
            printk("  [%u] ID=%u Type=%u Version=%u Present=%s Active=%s "
                   "Commands=%llu Errors=%llu\n",
                   i, dev->id, dev->type, dev->version,
                   dev->present ? "yes" : "no",
                   dev->active ? "yes" : "no",
                   (unsigned long long)dev->total_commands,
                   (unsigned long long)dev->total_errors);
        }
    }
}
