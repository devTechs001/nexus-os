/*
 * NEXUS OS - Security Framework
 * security/crypto/crypto.h
 *
 * Cryptographic API Header
 * Provides encryption, hashing, and public key operations
 */

#ifndef _NEXUS_CRYPTO_H
#define _NEXUS_CRYPTO_H

#include "../../kernel/include/kernel.h"
#include "../../kernel/sync/sync.h"

/*===========================================================================*/
/*                         CRYPTO CONSTANTS                                  */
/*===========================================================================*/

/* Crypto API Version */
#define CRYPTO_VERSION_MAJOR        1
#define CRYPTO_VERSION_MINOR        0
#define CRYPTO_VERSION_PATCH        0
#define CRYPTO_VERSION_STRING       "1.0.0"

/* Algorithm Types */
#define CRYPTO_ALG_NONE             0
#define CRYPTO_ALG_AES_128          1
#define CRYPTO_ALG_AES_192          2
#define CRYPTO_ALG_AES_256          3
#define CRYPTO_ALG_SHA256           4
#define CRYPTO_ALG_SHA512           5
#define CRYPTO_ALG_RSA_2048         6
#define CRYPTO_ALG_RSA_4096         7
#define CRYPTO_ALG_ECC_P256         8
#define CRYPTO_ALG_ECC_P384         9
#define CRYPTO_ALG_ECC_P521         10

/* Cipher Modes */
#define CRYPTO_MODE_ECB             0
#define CRYPTO_MODE_CBC             1
#define CRYPTO_MODE_CTR             2
#define CRYPTO_MODE_GCM             3
#define CRYPTO_MODE_XTS             4

/* Padding Types */
#define CRYPTO_PAD_NONE             0
#define CRYPTO_PAD_PKCS7            1
#define CRYPTO_PAD_PKCS5            2
#define CRYPTO_PAD_ZERO             3

/* Key Sizes */
#define AES_KEY_SIZE_128            16
#define AES_KEY_SIZE_192            24
#define AES_KEY_SIZE_256            32
#define AES_BLOCK_SIZE              16
#define AES_IV_SIZE                 16

#define SHA256_DIGEST_SIZE          32
#define SHA256_BLOCK_SIZE           64
#define SHA512_DIGEST_SIZE          64
#define SHA512_BLOCK_SIZE           128

#define RSA_KEY_SIZE_2048           256
#define RSA_KEY_SIZE_4096           512
#define RSA_MAX_KEY_SIZE            512

#define ECC_P256_KEY_SIZE           32
#define ECC_P384_KEY_SIZE           48
#define ECC_P521_KEY_SIZE           66
#define ECC_MAX_KEY_SIZE            66

/* Maximum Sizes */
#define CRYPTO_MAX_KEY_SIZE         64
#define CRYPTO_MAX_BLOCK_SIZE       128
#define CRYPTO_MAX_DIGEST_SIZE      64
#define CRYPTO_MAX_IV_SIZE          16
#define CRYPTO_MAX_TAG_SIZE         16

/* Crypto Operation Flags */
#define CRYPTO_FLAG_ENCRYPT         0x00000001
#define CRYPTO_FLAG_DECRYPT         0x00000002
#define CRYPTO_FLAG_SYNC            0x00000004
#define CRYPTO_FLAG_ASYNC           0x00000008
#define CRYPTO_FLAG_DMA             0x00000010

/* Error Codes */
#define CRYPTO_OK                   0
#define CRYPTO_ERR_GENERAL          (-1)
#define CRYPTO_ERR_INVALID_KEY      (-2)
#define CRYPTO_ERR_INVALID_IV       (-3)
#define CRYPTO_ERR_INVALID_INPUT    (-4)
#define CRYPTO_ERR_INVALID_OUTPUT   (-5)
#define CRYPTO_ERR_BUFFER_SMALL     (-6)
#define CRYPTO_ERR_PADDING          (-7)
#define CRYPTO_ERR_AUTH_FAILED      (-8)
#define CRYPTO_ERR_KEYGEN           (-9)
#define CRYPTO_ERR_SIGN             (-10)
#define CRYPTO_ERR_VERIFY           (-11)
#define CRYPTO_ERR_NO_MEMORY        (-12)

/* Magic Numbers */
#define CRYPTO_CTX_MAGIC            0x43525950544F0001ULL  /* CRYPTO01 */
#define CRYPTO_AES_MAGIC            0x4145534354580001ULL  /* AESCTX01 */
#define CRYPTO_SHA_MAGIC            0x5348414354580001ULL  /* SHACTX01 */
#define CRYPTO_RSA_MAGIC            0x5253414354580001ULL  /* RSACTX01 */
#define CRYPTO_ECC_MAGIC            0x4543434354580001ULL  /* ECCTX001 */

/*===========================================================================*/
/*                         CRYPTO DATA STRUCTURES                            */
/*===========================================================================*/

/**
 * aes_context_t - AES cipher context
 *
 * Holds the expanded key schedule and state for AES operations.
 */
typedef struct {
    u64 magic;                  /* Magic number for validation */
    spinlock_t lock;            /* Protection lock */
    u32 key_size;               /* Key size in bytes (16/24/32) */
    u32 rounds;                 /* Number of rounds (10/12/14) */
    u32 mode;                   /* Cipher mode */
    u8 key[CRYPTO_MAX_KEY_SIZE];    /* Encryption key */
    u8 iv[CRYPTO_MAX_IV_SIZE];      /* Initialization vector */
    u32 rk[60];                 /* Round key schedule */
    u8 buf[AES_BLOCK_SIZE];     /* Buffer for partial blocks */
    u32 buf_len;                /* Bytes in buffer */
} aes_context_t;

/**
 * sha_context_t - SHA hash context
 *
 * Holds the state for SHA-256/SHA-512 hash operations.
 */
typedef struct {
    u64 magic;                  /* Magic number for validation */
    spinlock_t lock;            /* Protection lock */
    u32 alg;                    /* Algorithm (SHA256/SHA512) */
    u32 block_size;             /* Block size */
    u32 digest_size;            /* Digest size */
    u64 total[2];               /* Total bytes processed */
    u8 buffer[SHA512_BLOCK_SIZE];   /* Data buffer */
    u32 buf_len;                /* Bytes in buffer */
    union {
        u32 state_sha256[8];    /* SHA-256 state */
        u64 state_sha512[8];    /* SHA-512 state */
    };
} sha_context_t;

/**
 * rsa_key_t - RSA key structure
 *
 * Holds RSA public or private key components.
 */
typedef struct {
    u32 size;                   /* Key size in bytes */
    u32 bits;                   /* Key size in bits */
    u8 *n;                      /* Modulus */
    u8 *e;                      /* Public exponent */
    u8 *d;                      /* Private exponent */
    u8 *p;                      /* Prime factor 1 */
    u8 *q;                      /* Prime factor 2 */
    u8 *dp;                     /* d mod (p-1) */
    u8 *dq;                     /* d mod (q-1) */
    u8 *qp;                     /* q^(-1) mod p */
} rsa_key_t;

/**
 * rsa_context_t - RSA cipher context
 *
 * Holds RSA key and state for RSA operations.
 */
typedef struct {
    u64 magic;                  /* Magic number for validation */
    spinlock_t lock;            /* Protection lock */
    rsa_key_t key;              /* RSA key */
    u32 padding;                /* Padding scheme */
    u8 *md_info;                /* Hash algorithm info */
} rsa_context_t;

/**
 * ecc_point_t - Elliptic curve point
 *
 * Represents a point on an elliptic curve.
 */
typedef struct {
    u32 size;                   /* Coordinate size in bytes */
    u8 *x;                      /* X coordinate */
    u8 *y;                      /* Y coordinate */
    u8 *z;                      /* Z coordinate (for projective) */
} ecc_point_t;

/**
 * ecc_key_t - ECC key structure
 *
 * Holds ECC public or private key.
 */
typedef struct {
    u32 curve;                  /* Curve type (P256/P384/P521) */
    u32 size;                   /* Key size in bytes */
    u8 *d;                      /* Private key (scalar) */
    ecc_point_t q;              /* Public key (point) */
} ecc_key_t;

/**
 * ecc_context_t - ECC cipher context
 *
 * Holds ECC key and state for ECC operations.
 */
typedef struct {
    u64 magic;                  /* Magic number for validation */
    spinlock_t lock;            /* Protection lock */
    ecc_key_t key;              /* ECC key */
    u32 curve;                  /* Curve type */
} ecc_context_t;

/**
 * crypto_context_t - General crypto context
 *
 * Unified context for crypto operations.
 */
typedef struct {
    u64 magic;                  /* Magic number for validation */
    spinlock_t lock;            /* Protection lock */
    u32 alg;                    /* Algorithm type */
    u32 mode;                   /* Operation mode */
    u32 flags;                  /* Operation flags */
    union {
        aes_context_t aes;      /* AES context */
        sha_context_t sha;      /* SHA context */
        rsa_context_t rsa;      /* RSA context */
        ecc_context_t ecc;      /* ECC context */
    };
    u64 operations;             /* Operation count */
    u64 bytes_processed;        /* Bytes processed */
} crypto_context_t;

/**
 * crypto_stats_t - Crypto statistics
 */
typedef struct {
    u64 total_operations;       /* Total operations */
    u64 encrypt_ops;            /* Encryption operations */
    u64 decrypt_ops;            /* Decryption operations */
    u64 hash_ops;               /* Hash operations */
    u64 sign_ops;               /* Sign operations */
    u64 verify_ops;             /* Verify operations */
    u64 keygen_ops;             /* Key generation operations */
    u64 bytes_encrypted;        /* Bytes encrypted */
    u64 bytes_decrypted;        /* Bytes decrypted */
    u64 bytes_hashed;           /* Bytes hashed */
    u64 errors;                 /* Error count */
} crypto_stats_t;

/*===========================================================================*/
/*                         CRYPTO API                                        */
/*===========================================================================*/

/* Initialization */
s32 crypto_init(void);
void crypto_shutdown(void);
crypto_context_t *crypto_alloc_context(u32 alg);
void crypto_free_context(crypto_context_t *ctx);

/* Context Management */
s32 crypto_set_key(crypto_context_t *ctx, const u8 *key, u32 key_len);
s32 crypto_set_iv(crypto_context_t *ctx, const u8 *iv, u32 iv_len);
s32 crypto_reset(crypto_context_t *ctx);

/* Statistics */
void crypto_get_stats(crypto_stats_t *stats);
void crypto_print_stats(void);

/*===========================================================================*/
/*                         AES API                                           */
/*===========================================================================*/

/* Context Management */
s32 aes_init(aes_context_t *ctx);
void aes_free(aes_context_t *ctx);
s32 aes_set_key(aes_context_t *ctx, const u8 *key, u32 key_bits);
s32 aes_set_iv(aes_context_t *ctx, const u8 *iv);
s32 aes_set_mode(aes_context_t *ctx, u32 mode);

/* Block Operations */
s32 aes_encrypt_block(aes_context_t *ctx, const u8 *input, u8 *output);
s32 aes_decrypt_block(aes_context_t *ctx, const u8 *input, u8 *output);

/* Stream Operations */
s32 aes_encrypt(aes_context_t *ctx, const u8 *input, u8 *output, u32 length);
s32 aes_decrypt(aes_context_t *ctx, const u8 *input, u8 *output, u32 length);

/* One-shot Operations */
s32 aes_encrypt_cbc(const u8 *key, u32 key_bits, const u8 *iv,
                    const u8 *input, u8 *output, u32 length);
s32 aes_decrypt_cbc(const u8 *key, u32 key_bits, const u8 *iv,
                    const u8 *input, u8 *output, u32 length);
s32 aes_encrypt_ctr(const u8 *key, u32 key_bits, const u8 *nonce,
                    const u8 *input, u8 *output, u32 length);
s32 aes_decrypt_ctr(const u8 *key, u32 key_bits, const u8 *nonce,
                    const u8 *input, u8 *output, u32 length);

/*===========================================================================*/
/*                         SHA API                                           */
/*===========================================================================*/

/* Context Management */
s32 sha256_init(sha_context_t *ctx);
s32 sha512_init(sha_context_t *ctx);
void sha_free(sha_context_t *ctx);
s32 sha_starts(sha_context_t *ctx);
s32 sha_update(sha_context_t *ctx, const u8 *input, u32 length);
s32 sha_finish(sha_context_t *ctx, u8 *output);

/* One-shot Operations */
s32 sha256(const u8 *input, u32 length, u8 *output);
s32 sha512(const u8 *input, u32 length, u8 *output);

/* HMAC Operations */
s32 hmac_sha256(const u8 *key, u32 key_len, const u8 *input, u32 input_len, u8 *output);
s32 hmac_sha512(const u8 *key, u32 key_len, const u8 *input, u32 input_len, u8 *output);

/* KDF Operations */
s32 pbkdf2_sha256(const u8 *password, u32 pass_len, const u8 *salt, u32 salt_len,
                  u32 iterations, u8 *output, u32 output_len);

/*===========================================================================*/
/*                         RSA API                                           */
/*===========================================================================*/

/* Context Management */
s32 rsa_init(rsa_context_t *ctx);
void rsa_free(rsa_context_t *ctx);
s32 rsa_set_key(rsa_context_t *ctx, const rsa_key_t *key);

/* Key Operations */
s32 rsa_generate_key(rsa_context_t *ctx, u32 bits, u32 exponent);
s32 rsa_import_public_key(rsa_context_t *ctx, const u8 *n, u32 n_len,
                          const u8 *e, u32 e_len);
s32 rsa_import_private_key(rsa_context_t *ctx, const u8 *n, u32 n_len,
                           const u8 *d, u32 d_len);

/* Encryption/Decryption */
s32 rsa_encrypt(rsa_context_t *ctx, const u8 *input, u8 *output, u32 *output_len);
s32 rsa_decrypt(rsa_context_t *ctx, const u8 *input, u8 *output, u32 *output_len);

/* Signing/Verification */
s32 rsa_sign(rsa_context_t *ctx, const u8 *hash, u32 hash_len,
             u8 *signature, u32 *sig_len);
s32 rsa_verify(rsa_context_t *ctx, const u8 *hash, u32 hash_len,
               const u8 *signature, u32 sig_len);

/* One-shot Operations */
s32 rsa_encrypt_pkcs1(const rsa_key_t *key, const u8 *input, u32 input_len,
                      u8 *output, u32 *output_len);
s32 rsa_decrypt_pkcs1(const rsa_key_t *key, const u8 *input, u32 input_len,
                      u8 *output, u32 *output_len);

/*===========================================================================*/
/*                         ECC API                                           */
/*===========================================================================*/

/* Context Management */
s32 ecc_init(ecc_context_t *ctx, u32 curve);
void ecc_free(ecc_context_t *ctx);
s32 ecc_set_key(ecc_context_t *ctx, const ecc_key_t *key);

/* Key Operations */
s32 ecc_generate_key(ecc_context_t *ctx);
s32 ecc_compute_public_key(ecc_context_t *ctx);

/* Key Exchange */
s32 ecc_shared_secret(ecc_context_t *ctx, const ecc_point_t *point,
                      u8 *secret, u32 *secret_len);

/* Signing/Verification */
s32 ecc_sign(ecc_context_t *ctx, const u8 *hash, u32 hash_len,
             u8 *signature, u32 *sig_len);
s32 ecc_verify(ecc_context_t *ctx, const u8 *hash, u32 hash_len,
               const u8 *signature, u32 sig_len);

/* Point Operations */
s32 ecc_point_init(ecc_point_t *point, u32 size);
void ecc_point_free(ecc_point_t *point);
s32 ecc_point_add(ecc_point_t *r, const ecc_point_t *a, const ecc_point_t *b);
s32 ecc_point_multiply(ecc_point_t *r, const ecc_point_t *p, const u8 *scalar, u32 scalar_len);

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/* Random Generation */
s32 crypto_random_bytes(u8 *buffer, u32 length);
s32 crypto_random_u32(u32 *value);
s32 crypto_random_u64(u64 *value);

/* Memory Utilities */
void crypto_memzero(void *ptr, u32 size);
s32 crypto_memcmp(const void *a, const void *b, u32 size);

/* Encoding/Decoding */
s32 base64_encode(const u8 *input, u32 input_len, u8 *output, u32 *output_len);
s32 base64_decode(const u8 *input, u32 input_len, u8 *output, u32 *output_len);

/* Hex Utilities */
s32 hex_to_bytes(const char *hex, u8 *bytes, u32 *bytes_len);
s32 bytes_to_hex(const u8 *bytes, u32 bytes_len, char *hex, u32 *hex_len);

#endif /* _NEXUS_CRYPTO_H */
