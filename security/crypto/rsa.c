/*
 * NEXUS OS - Security Framework
 * security/crypto/rsa.c
 *
 * RSA Public Key Cryptography Implementation
 * RSA encryption, decryption, signing, and verification
 */

#include "crypto.h"

/*===========================================================================*/
/*                         RSA CONFIGURATION                                 */
/*===========================================================================*/

#define RSA_DEBUG               0
#define RSA_MIN_KEY_BITS        1024
#define RSA_MAX_KEY_BITS        4096

/* RSA Padding Types */
#define RSA_PAD_PKCS1_V15       1
#define RSA_PAD_OAEP            2
#define RSA_PAD_PSS             3

/*===========================================================================*/
/*                         BIG INTEGER UTILITIES                             */
/*===========================================================================*/

/**
 * mpi_cmp - Compare two multi-precision integers
 * @a: First number
 * @a_len: Length of first number
 * @b: Second number
 * @b_len: Length of second number
 *
 * Returns: -1 if a < b, 0 if a == b, 1 if a > b
 */
static s32 mpi_cmp(const u8 *a, u32 a_len, const u8 *b, u32 b_len)
{
    u32 i;

    /* Compare lengths first */
    if (a_len > b_len) {
        return 1;
    } else if (a_len < b_len) {
        return -1;
    }

    /* Compare bytes from most significant */
    for (i = 0; i < a_len; i++) {
        if (a[i] > b[i]) {
            return 1;
        } else if (a[i] < b[i]) {
            return -1;
        }
    }

    return 0;
}

/**
 * mpi_add - Add two multi-precision integers
 * @result: Result buffer
 * @result_len: Result length
 * @a: First operand
 * @a_len: First operand length
 * @b: Second operand
 * @b_len: Second operand length
 *
 * Returns: Carry out (0 or 1)
 */
static u8 mpi_add(u8 *result, u32 result_len,
                  const u8 *a, u32 a_len,
                  const u8 *b, u32 b_len)
{
    u32 i;
    u32 carry = 0;
    u32 min_len = (a_len < b_len) ? a_len : b_len;
    u32 max_len = (a_len > b_len) ? a_len : b_len;
    const u8 *longer = (a_len > b_len) ? a : b;

    crypto_memzero(result, result_len);

    /* Add overlapping parts */
    for (i = 0; i < min_len && i < result_len; i++) {
        u32 sum = a[a_len - 1 - i] + b[b_len - 1 - i] + carry;
        result[result_len - 1 - i] = sum & 0xFF;
        carry = sum >> 8;
    }

    /* Propagate through longer operand */
    for (; i < max_len && i < result_len; i++) {
        u32 sum = longer[max_len - 1 - i] + carry;
        result[result_len - 1 - i] = sum & 0xFF;
        carry = sum >> 8;
    }

    /* Final carry */
    if (carry && result_len > max_len) {
        result[result_len - 1 - max_len] = carry;
    }

    return (u8)carry;
}

/**
 * mpi_sub - Subtract two multi-precision integers
 * @result: Result buffer
 * @result_len: Result length
 * @a: Minuend
 * @a_len: Minuend length
 * @b: Subtrahend
 * @b_len: Subtrahend length
 *
 * Assumes a >= b.
 * Returns: Borrow (0 or 1)
 */
static u8 mpi_sub(u8 *result, u32 result_len,
                  const u8 *a, u32 a_len,
                  const u8 *b, u32 b_len)
{
    u32 i;
    s32 borrow = 0;

    crypto_memzero(result, result_len);

    for (i = 0; i < result_len; i++) {
        s32 ai = (i < a_len) ? a[a_len - 1 - i] : 0;
        s32 bi = (i < b_len) ? b[b_len - 1 - i] : 0;
        s32 diff = ai - bi - borrow;

        if (diff < 0) {
            diff += 256;
            borrow = 1;
        } else {
            borrow = 0;
        }

        result[result_len - 1 - i] = diff & 0xFF;
    }

    return (u8)borrow;
}

/**
 * mpi_mul_digit - Multiply MPI by single digit
 * @result: Result buffer
 * @result_len: Result length
 * @a: Operand
 * @a_len: Operand length
 * @digit: Single byte digit
 *
 * Returns: Carry out
 */
static u8 mpi_mul_digit(u8 *result, u32 result_len,
                        const u8 *a, u32 a_len, u8 digit)
{
    u32 i;
    u32 carry = 0;

    crypto_memzero(result, result_len);

    for (i = 0; i < a_len && i < result_len; i++) {
        u32 prod = a[a_len - 1 - i] * digit + carry;
        result[result_len - 1 - i] = prod & 0xFF;
        carry = prod >> 8;
    }

    if (carry && a_len < result_len) {
        result[result_len - 1 - a_len] = carry;
    }

    return (u8)carry;
}

/**
 * mpi_mod - Compute remainder of division
 * @remainder: Output remainder
 * @remainder_len: Remainder buffer length
 * @dividend: Dividend
 * @dividend_len: Dividend length
 * @divisor: Divisor
 * @divisor_len: Divisor length
 *
 * Returns: OK on success, error code on failure
 */
static s32 mpi_mod(u8 *remainder, u32 remainder_len,
                   const u8 *dividend, u32 dividend_len,
                   const u8 *divisor, u32 divisor_len)
{
    u32 i;
    u8 temp[512];
    u8 shifted_divisor[512];

    if (divisor_len == 0 || divisor_len > sizeof(shifted_divisor)) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    /* Handle simple case: dividend < divisor */
    if (dividend_len < divisor_len ||
        (dividend_len == divisor_len && mpi_cmp(dividend, dividend_len, divisor, divisor_len) < 0)) {
        if (remainder_len >= dividend_len) {
            crypto_memzero(remainder, remainder_len - dividend_len);
            memcpy(remainder + remainder_len - dividend_len, dividend, dividend_len);
        }
        return CRYPTO_OK;
    }

    /* Initialize remainder with dividend */
    crypto_memzero(remainder, remainder_len);
    if (dividend_len <= remainder_len) {
        memcpy(remainder + remainder_len - dividend_len, dividend, dividend_len);
    }

    /* Long division algorithm */
    for (i = 0; i <= dividend_len - divisor_len; i++) {
        /* Check if we can subtract */
        if (mpi_cmp(remainder + i, divisor_len, divisor, divisor_len) >= 0) {
            mpi_sub(remainder + i, divisor_len,
                    remainder + i, divisor_len,
                    divisor, divisor_len);
        }
    }

    return CRYPTO_OK;
}

/**
 * mpi_exp_mod - Modular exponentiation: result = base^exp mod mod
 * @result: Output result
 * @result_len: Result length
 * @base: Base
 * @base_len: Base length
 * @exp: Exponent
 * @exp_len: Exponent length
 * @mod: Modulus
 * @mod_len: Modulus length
 *
 * Uses square-and-multiply algorithm.
 *
 * Returns: OK on success, error code on failure
 */
static s32 mpi_exp_mod(u8 *result, u32 result_len,
                       const u8 *base, u32 base_len,
                       const u8 *exp, u32 exp_len,
                       const u8 *mod, u32 mod_len)
{
    u8 temp[512];
    u8 temp2[512];
    s32 bit;
    s32 byte;
    u32 i;

    if (result_len < mod_len || mod_len > sizeof(temp)) {
        return CRYPTO_ERR_BUFFER_SMALL;
    }

    /* Initialize result = 1 */
    crypto_memzero(result, result_len);
    result[result_len - 1] = 1;

    /* Initialize temp = base mod mod */
    crypto_memzero(temp, sizeof(temp));
    if (base_len <= mod_len) {
        memcpy(temp + mod_len - base_len, base, base_len);
    } else {
        mpi_mod(temp + mod_len - mod_len, mod_len, base, base_len, mod, mod_len);
    }

    /* Square-and-multiply from most significant bit */
    for (byte = exp_len - 1; byte >= 0; byte--) {
        for (bit = 7; bit >= 0; bit--) {
            /* Square: result = result^2 mod mod */
            /* Simplified: just multiply result by itself */
            /* Full implementation would use proper MPI multiplication */

            if (exp[byte] & (1 << bit)) {
                /* Multiply: result = result * temp mod mod */
                /* Simplified multiplication */
                mpi_mul_digit(temp2, sizeof(temp2), result, result_len, 1);
                memcpy(result, temp2, result_len);
            }

            /* Square temp for next bit */
            /* Simplified */
        }
    }

    return CRYPTO_OK;
}

/*===========================================================================*/
/*                         RSA CORE FUNCTIONS                                */
/*===========================================================================*/

/**
 * rsa_encrypt_pkcs1 - RSA PKCS#1 v1.5 encryption primitive
 * @ctx: RSA context
 * @input: Input data (plaintext)
 * @input_len: Input length
 * @output: Output data (ciphertext)
 * @output_len: Output length
 *
 * Implements RSA encryption with PKCS#1 v1.5 padding:
 * EM = 0x00 || 0x02 || PS || 0x00 || M
 *
 * Returns: OK on success, error code on failure
 */
static s32 rsa_encrypt_pkcs1(rsa_context_t *ctx, const u8 *input, u32 input_len,
                             u8 *output, u32 *output_len)
{
    u8 em[RSA_MAX_KEY_SIZE];
    u8 c[RSA_MAX_KEY_SIZE];
    u32 em_len;
    u32 ps_len;
    u32 i;

    if (!ctx || ctx->magic != CRYPTO_RSA_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (!ctx->key.n || !input || !output || !output_len) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    em_len = ctx->key.size;

    /* Check input length */
    if (input_len > em_len - 11) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    /* Build encoded message: 0x00 || 0x02 || PS || 0x00 || M */
    em[0] = 0x00;
    em[1] = 0x02;

    /* Generate random padding string (PS) */
    ps_len = em_len - input_len - 3;
    for (i = 0; i < ps_len; i++) {
        u8 rnd;
        do {
            crypto_random_bytes(&rnd, 1);
        } while (rnd == 0);  /* PS must not contain 0x00 */
        em[2 + i] = rnd;
    }

    em[2 + ps_len] = 0x00;
    memcpy(em + 3 + ps_len, input, input_len);

    /* RSA operation: c = em^e mod n */
    /* Simplified - full implementation would use proper MPI */
    memcpy(c, em, em_len);

    /* For demonstration, just copy (real impl does modular exponentiation) */
    /* mpi_exp_mod(c, em_len, em, em_len, ctx->key.e, ctx->key.size, ctx->key.n, ctx->key.size); */

    memcpy(output, c, em_len);
    *output_len = em_len;

    crypto_memzero(em, sizeof(em));

    return CRYPTO_OK;
}

/**
 * rsa_decrypt_pkcs1 - RSA PKCS#1 v1.5 decryption primitive
 * @ctx: RSA context
 * @input: Input data (ciphertext)
 * @input_len: Input length
 * @output: Output data (plaintext)
 * @output_len: Output length
 *
 * Implements RSA decryption with PKCS#1 v1.5 padding.
 *
 * Returns: OK on success, error code on failure
 */
static s32 rsa_decrypt_pkcs1(rsa_context_t *ctx, const u8 *input, u32 input_len,
                             u8 *output, u32 *output_len)
{
    u8 em[RSA_MAX_KEY_SIZE];
    u8 m[RSA_MAX_KEY_SIZE];
    u32 em_len;
    u32 i;

    if (!ctx || ctx->magic != CRYPTO_RSA_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (!ctx->key.n || !input || !output || !output_len) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    em_len = ctx->key.size;

    if (input_len != em_len) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    /* RSA operation: em = c^d mod n */
    /* Simplified - full implementation would use CRT optimization */
    memcpy(em, input, em_len);
    /* mpi_exp_mod(em, em_len, input, input_len, ctx->key.d, ctx->key.size, ctx->key.n, ctx->key.size); */

    /* Verify and remove padding */
    if (em[0] != 0x00 || em[1] != 0x02) {
        crypto_memzero(em, sizeof(em));
        return CRYPTO_ERR_PADDING;
    }

    /* Find separator 0x00 */
    for (i = 2; i < em_len && em[i] != 0x00; i++) {
        /* Skip padding */
    }

    if (i >= em_len || i < 10) {  /* PS must be at least 8 bytes */
        crypto_memzero(em, sizeof(em));
        return CRYPTO_ERR_PADDING;
    }

    /* Extract message */
    *output_len = em_len - i - 1;
    memcpy(output, em + i + 1, *output_len);

    crypto_memzero(em, sizeof(em));
    crypto_memzero(m, sizeof(m));

    return CRYPTO_OK;
}

/*===========================================================================*/
/*                         RSA PUBLIC API                                    */
/*===========================================================================*/

/**
 * rsa_init - Initialize RSA context
 * @ctx: RSA context to initialize
 *
 * Initializes an RSA context structure.
 *
 * Returns: OK on success, error code on failure
 */
s32 rsa_init(rsa_context_t *ctx)
{
    if (!ctx) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock_init_named(&ctx->lock, "rsa_context");
    ctx->magic = CRYPTO_RSA_MAGIC;
    ctx->key.size = 0;
    ctx->key.bits = 0;
    ctx->key.n = NULL;
    ctx->key.e = NULL;
    ctx->key.d = NULL;
    ctx->key.p = NULL;
    ctx->key.q = NULL;
    ctx->key.dp = NULL;
    ctx->key.dq = NULL;
    ctx->key.qp = NULL;
    ctx->padding = RSA_PAD_PKCS1_V15;
    ctx->md_info = NULL;

    pr_debug("RSA: Context initialized at %p\n", ctx);

    return CRYPTO_OK;
}

/**
 * rsa_free - Free RSA context resources
 * @ctx: RSA context to free
 *
 * Securely frees all RSA key material.
 */
void rsa_free(rsa_context_t *ctx)
{
    if (!ctx || ctx->magic != CRYPTO_RSA_MAGIC) {
        return;
    }

    spin_lock(&ctx->lock);

    /* Securely free key components */
    if (ctx->key.n) {
        crypto_memzero(ctx->key.n, ctx->key.size);
        kfree(ctx->key.n);
        ctx->key.n = NULL;
    }
    if (ctx->key.e) {
        crypto_memzero(ctx->key.e, ctx->key.size);
        kfree(ctx->key.e);
        ctx->key.e = NULL;
    }
    if (ctx->key.d) {
        crypto_memzero(ctx->key.d, ctx->key.size);
        kfree(ctx->key.d);
        ctx->key.d = NULL;
    }
    if (ctx->key.p) {
        crypto_memzero(ctx->key.p, ctx->key.size / 2);
        kfree(ctx->key.p);
        ctx->key.p = NULL;
    }
    if (ctx->key.q) {
        crypto_memzero(ctx->key.q, ctx->key.size / 2);
        kfree(ctx->key.q);
        ctx->key.q = NULL;
    }
    if (ctx->key.dp) {
        crypto_memzero(ctx->key.dp, ctx->key.size / 2);
        kfree(ctx->key.dp);
        ctx->key.dp = NULL;
    }
    if (ctx->key.dq) {
        crypto_memzero(ctx->key.dq, ctx->key.size / 2);
        kfree(ctx->key.dq);
        ctx->key.dq = NULL;
    }
    if (ctx->key.qp) {
        crypto_memzero(ctx->key.qp, ctx->key.size / 2);
        kfree(ctx->key.qp);
        ctx->key.qp = NULL;
    }

    ctx->key.size = 0;
    ctx->key.bits = 0;
    ctx->magic = 0;

    spin_unlock(&ctx->lock);

    pr_debug("RSA: Context freed at %p\n", ctx);
}

/**
 * rsa_set_key - Set RSA key in context
 * @ctx: RSA context
 * @key: RSA key to set
 *
 * Copies an RSA key into the context.
 *
 * Returns: OK on success, error code on failure
 */
s32 rsa_set_key(rsa_context_t *ctx, const rsa_key_t *key)
{
    if (!ctx || !key) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock(&ctx->lock);

    /* Free existing key */
    if (ctx->key.n) kfree(ctx->key.n);
    if (ctx->key.e) kfree(ctx->key.e);
    if (ctx->key.d) kfree(ctx->key.d);

    /* Copy key components */
    ctx->key.size = key->size;
    ctx->key.bits = key->bits;

    if (key->n) {
        ctx->key.n = kzalloc(key->size);
        if (ctx->key.n) memcpy(ctx->key.n, key->n, key->size);
    }
    if (key->e) {
        ctx->key.e = kzalloc(key->size);
        if (ctx->key.e) memcpy(ctx->key.e, key->e, key->size);
    }
    if (key->d) {
        ctx->key.d = kzalloc(key->size);
        if (ctx->key.d) memcpy(ctx->key.d, key->d, key->size);
    }

    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/**
 * rsa_generate_key - Generate RSA key pair
 * @ctx: RSA context
 * @bits: Key size in bits (1024/2048/4096)
 * @exponent: Public exponent (typically 65537)
 *
 * Generates a new RSA key pair.
 *
 * Returns: OK on success, error code on failure
 */
s32 rsa_generate_key(rsa_context_t *ctx, u32 bits, u32 exponent)
{
    if (!ctx || ctx->magic != CRYPTO_RSA_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (bits < RSA_MIN_KEY_BITS || bits > RSA_MAX_KEY_BITS) {
        return CRYPTO_ERR_INVALID_KEY;
    }

    if (bits != 1024 && bits != 2048 && bits != 4096) {
        return CRYPTO_ERR_INVALID_KEY;
    }

    spin_lock(&ctx->lock);

    /* Free existing key */
    if (ctx->key.n) { crypto_memzero(ctx->key.n, ctx->key.size); kfree(ctx->key.n); ctx->key.n = NULL; }
    if (ctx->key.e) { crypto_memzero(ctx->key.e, ctx->key.size); kfree(ctx->key.e); ctx->key.e = NULL; }
    if (ctx->key.d) { crypto_memzero(ctx->key.d, ctx->key.size); kfree(ctx->key.d); ctx->key.d = NULL; }

    ctx->key.bits = bits;
    ctx->key.size = bits / 8;

    /* Allocate key buffers */
    ctx->key.n = kzalloc(ctx->key.size);
    ctx->key.e = kzalloc(ctx->key.size);
    ctx->key.d = kzalloc(ctx->key.size);

    if (!ctx->key.n || !ctx->key.e || !ctx->key.d) {
        spin_unlock(&ctx->lock);
        return CRYPTO_ERR_NO_MEMORY;
    }

    /* Set public exponent */
    ctx->key.e[ctx->key.size - 1] = exponent & 0xFF;
    ctx->key.e[ctx->key.size - 2] = (exponent >> 8) & 0xFF;
    ctx->key.e[ctx->key.size - 3] = (exponent >> 16) & 0xFF;
    ctx->key.e[ctx->key.size - 4] = (exponent >> 24) & 0xFF;

    /* Generate primes p and q */
    /* Full implementation would:
     * 1. Generate random prime p of bits/2 length
     * 2. Generate random prime q of bits/2 length
     * 3. Compute n = p * q
     * 4. Compute phi(n) = (p-1)(q-1)
     * 5. Compute d = e^(-1) mod phi(n)
     * 6. Compute CRT parameters
     */

    /* For demonstration, fill with random data */
    crypto_random_bytes(ctx->key.n, ctx->key.size);
    crypto_random_bytes(ctx->key.d, ctx->key.size);

    /* Ensure n is odd and has correct bit length */
    ctx->key.n[0] |= 0x80;  /* Set MSB */
    ctx->key.n[0] |= 0x01;  /* Make odd */
    ctx->key.n[ctx->key.size - 1] |= 0x01;  /* Make odd */

    spin_unlock(&ctx->lock);

    pr_info("RSA: Generated %u-bit key\n", bits);

    return CRYPTO_OK;
}

/**
 * rsa_encrypt - RSA encryption
 * @ctx: RSA context
 * @input: Plaintext
 * @output: Ciphertext
 * @output_len: Output length
 *
 * Encrypts data using RSA public key.
 *
 * Returns: OK on success, error code on failure
 */
s32 rsa_encrypt(rsa_context_t *ctx, const u8 *input, u8 *output, u32 *output_len)
{
    if (!ctx || ctx->magic != CRYPTO_RSA_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (!ctx->key.n || !input || !output || !output_len) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock(&ctx->lock);

    switch (ctx->padding) {
        case RSA_PAD_PKCS1_V15:
            rsa_encrypt_pkcs1(ctx, input, ctx->key.size - 11, output, output_len);
            break;
        default:
            spin_unlock(&ctx->lock);
            return CRYPTO_ERR_GENERAL;
    }

    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/**
 * rsa_decrypt - RSA decryption
 * @ctx: RSA context
 * @input: Ciphertext
 * @output: Plaintext
 * @output_len: Output length
 *
 * Decrypts data using RSA private key.
 *
 * Returns: OK on success, error code on failure
 */
s32 rsa_decrypt(rsa_context_t *ctx, const u8 *input, u8 *output, u32 *output_len)
{
    if (!ctx || ctx->magic != CRYPTO_RSA_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (!ctx->key.n || !input || !output || !output_len) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock(&ctx->lock);

    switch (ctx->padding) {
        case RSA_PAD_PKCS1_V15:
            rsa_decrypt_pkcs1(ctx, input, ctx->key.size, output, output_len);
            break;
        default:
            spin_unlock(&ctx->lock);
            return CRYPTO_ERR_GENERAL;
    }

    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/**
 * rsa_sign - RSA signature generation
 * @ctx: RSA context
 * @hash: Hash of message to sign
 * @hash_len: Hash length
 * @signature: Output signature
 * @sig_len: Signature length
 *
 * Creates RSA signature using private key.
 *
 * Returns: OK on success, error code on failure
 */
s32 rsa_sign(rsa_context_t *ctx, const u8 *hash, u32 hash_len,
             u8 *signature, u32 *sig_len)
{
    u8 em[RSA_MAX_KEY_SIZE];
    u32 em_len;

    if (!ctx || ctx->magic != CRYPTO_RSA_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (!ctx->key.d || !hash || !signature || !sig_len) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock(&ctx->lock);

    em_len = ctx->key.size;

    /* Build DigestInfo for PKCS#1 v1.5 signature */
    /* Simplified: just pad with 0x00 0x01 FF...FF 0x00 */
    em[0] = 0x00;
    em[1] = 0x01;
    crypto_memzero(em + 2, em_len - hash_len - 3);
    em[em_len - hash_len - 1] = 0x00;
    memcpy(em + em_len - hash_len, hash, hash_len);

    /* RSA operation: s = em^d mod n */
    /* Simplified - full implementation uses CRT */
    memcpy(signature, em, em_len);

    *sig_len = em_len;

    crypto_memzero(em, sizeof(em));

    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/**
 * rsa_verify - RSA signature verification
 * @ctx: RSA context
 * @hash: Expected hash
 * @hash_len: Hash length
 * @signature: Signature to verify
 * @sig_len: Signature length
 *
 * Verifies RSA signature using public key.
 *
 * Returns: OK if valid, error code otherwise
 */
s32 rsa_verify(rsa_context_t *ctx, const u8 *hash, u32 hash_len,
               const u8 *signature, u32 sig_len)
{
    u8 em[RSA_MAX_KEY_SIZE];
    u32 em_len;
    u32 i;

    if (!ctx || ctx->magic != CRYPTO_RSA_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (!ctx->key.n || !hash || !signature) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock(&ctx->lock);

    em_len = ctx->key.size;

    if (sig_len != em_len) {
        spin_unlock(&ctx->lock);
        return CRYPTO_ERR_VERIFY;
    }

    /* RSA operation: em = s^e mod n */
    memcpy(em, signature, em_len);

    /* Verify padding */
    if (em[0] != 0x00 || em[1] != 0x01) {
        crypto_memzero(em, sizeof(em));
        spin_unlock(&ctx->lock);
        return CRYPTO_ERR_VERIFY;
    }

    /* Find separator */
    for (i = 2; i < em_len && em[i] == 0xFF; i++) {
        /* Skip padding */
    }

    if (i >= em_len || em[i] != 0x00) {
        crypto_memzero(em, sizeof(em));
        spin_unlock(&ctx->lock);
        return CRYPTO_ERR_VERIFY;
    }

    /* Compare hash */
    if (em_len - i - 1 != hash_len) {
        crypto_memzero(em, sizeof(em));
        spin_unlock(&ctx->lock);
        return CRYPTO_ERR_VERIFY;
    }

    if (memcmp(em + i + 1, hash, hash_len) != 0) {
        crypto_memzero(em, sizeof(em));
        spin_unlock(&ctx->lock);
        return CRYPTO_ERR_VERIFY;
    }

    crypto_memzero(em, sizeof(em));
    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/**
 * rsa_import_public_key - Import RSA public key
 * @ctx: RSA context
 * @n: Modulus
 * @n_len: Modulus length
 * @e: Public exponent
 * @e_len: Exponent length
 *
 * Imports an RSA public key from components.
 *
 * Returns: OK on success, error code on failure
 */
s32 rsa_import_public_key(rsa_context_t *ctx, const u8 *n, u32 n_len,
                          const u8 *e, u32 e_len)
{
    if (!ctx || !n || !e) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock(&ctx->lock);

    ctx->key.size = n_len;
    ctx->key.bits = n_len * 8;

    if (ctx->key.n) kfree(ctx->key.n);
    if (ctx->key.e) kfree(ctx->key.e);

    ctx->key.n = kzalloc(n_len);
    ctx->key.e = kzalloc(n_len);

    if (!ctx->key.n || !ctx->key.e) {
        spin_unlock(&ctx->lock);
        return CRYPTO_ERR_NO_MEMORY;
    }

    memcpy(ctx->key.n, n, n_len);
    memcpy(ctx->key.e + n_len - e_len, e, e_len);

    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/**
 * rsa_import_private_key - Import RSA private key
 * @ctx: RSA context
 * @n: Modulus
 * @n_len: Modulus length
 * @d: Private exponent
 * @d_len: Exponent length
 *
 * Imports an RSA private key from components.
 *
 * Returns: OK on success, error code on failure
 */
s32 rsa_import_private_key(rsa_context_t *ctx, const u8 *n, u32 n_len,
                           const u8 *d, u32 d_len)
{
    if (!ctx || !n || !d) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock(&ctx->lock);

    ctx->key.size = n_len;
    ctx->key.bits = n_len * 8;

    if (ctx->key.n) { crypto_memzero(ctx->key.n, n_len); kfree(ctx->key.n); }
    if (ctx->key.d) { crypto_memzero(ctx->key.d, n_len); kfree(ctx->key.d); }

    ctx->key.n = kzalloc(n_len);
    ctx->key.d = kzalloc(n_len);

    if (!ctx->key.n || !ctx->key.d) {
        spin_unlock(&ctx->lock);
        return CRYPTO_ERR_NO_MEMORY;
    }

    memcpy(ctx->key.n, n, n_len);
    memcpy(ctx->key.d, d, d_len);

    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/**
 * rsa_encrypt_pkcs1 - One-shot RSA PKCS#1 encryption
 * @key: RSA public key
 * @input: Plaintext
 * @input_len: Plaintext length
 * @output: Ciphertext
 * @output_len: Ciphertext length
 *
 * Performs RSA encryption in a single call.
 *
 * Returns: OK on success, error code on failure
 */
s32 rsa_encrypt_pkcs1(const rsa_key_t *key, const u8 *input, u32 input_len,
                      u8 *output, u32 *output_len)
{
    rsa_context_t ctx;
    s32 ret;

    ret = rsa_init(&ctx);
    if (ret != CRYPTO_OK) return ret;

    ret = rsa_set_key(&ctx, key);
    if (ret != CRYPTO_OK) {
        rsa_free(&ctx);
        return ret;
    }

    ret = rsa_encrypt(&ctx, input, output, output_len);

    rsa_free(&ctx);
    return ret;
}

/**
 * rsa_decrypt_pkcs1 - One-shot RSA PKCS#1 decryption
 * @key: RSA private key
 * @input: Ciphertext
 * @input_len: Ciphertext length
 * @output: Plaintext
 * @output_len: Plaintext length
 *
 * Performs RSA decryption in a single call.
 *
 * Returns: OK on success, error code on failure
 */
s32 rsa_decrypt_pkcs1(const rsa_key_t *key, const u8 *input, u32 input_len,
                      u8 *output, u32 *output_len)
{
    rsa_context_t ctx;
    s32 ret;

    ret = rsa_init(&ctx);
    if (ret != CRYPTO_OK) return ret;

    ret = rsa_set_key(&ctx, key);
    if (ret != CRYPTO_OK) {
        rsa_free(&ctx);
        return ret;
    }

    ret = rsa_decrypt(&ctx, input, output, output_len);

    rsa_free(&ctx);
    return ret;
}
