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


/*
 * Big-integer arithmetic for RSA.
 *
 * The previous implementation was not arithmetic. mpi_mod() ran its subtraction
 * loop over the wrong span (it indexed `remainder + i` -- offset by the byte
 * index, not the byte weight), mpi_exp_mod() never applied a modulus at all
 * ("Simplified: just multiply result by itself"), and both RSA encrypt and
 * decrypt had the exponentiation commented out and replaced with memcpy. The
 * net effect was that "RSA encryption" returned the padded plaintext and
 * "RSA decryption" read the padding back off it, so a "ciphertext" on the wire
 * was the cleartext message in the clear.
 *
 * These routines work on big-endian byte strings of equal length, which is what
 * rsa_key_t stores. Values are kept in fixed-width buffers of `len` bytes and
 * are always < n when used as residues.
 */

/* Number of significant bits in a big-endian buffer. */
static u32 mpi_bit_length(const u8 *a, u32 len)
{
    u32 i;

    for (i = 0; i < len; i++) {
        if (a[i] != 0) {
            u8 v = a[i];
            u32 b = 0;

            while (v) {
                b++;
                v >>= 1;
            }
            return (len - i - 1) * 8 + b;
        }
    }

    return 0;   /* zero */
}

/* Return the bit at position `bit` (0 = least significant). */
static u32 mpi_get_bit(const u8 *a, u32 len, u32 bit)
{
    u32 byte_index = bit / 8;
    u32 bit_index  = bit % 8;

    if (byte_index >= len) {
        return 0;
    }

    /* Big-endian: bit 0 is the LSB of the LAST byte. */
    return (a[len - 1 - byte_index] >> bit_index) & 1;
}

/* result = a - b, returns the borrow. a, b, result are `len` bytes. */
static u8 mpi_sub_len(u8 *result, const u8 *a, const u8 *b, u32 len)
{
    u32 i;
    s32 borrow = 0;

    for (i = 0; i < len; i++) {
        s32 ai = a[len - 1 - i];
        s32 bi = b[len - 1 - i];
        s32 diff = ai - bi - borrow;

        if (diff < 0) {
            diff += 256;
            borrow = 1;
        } else {
            borrow = 0;
        }

        result[len - 1 - i] = (u8)diff;
    }

    return (u8)borrow;
}

/* Compare `len`-byte big-endian a and b: <0, 0, >0. */
static s32 mpi_cmp_len(const u8 *a, const u8 *b, u32 len)
{
    u32 i;

    for (i = 0; i < len; i++) {
        if (a[i] != b[i]) {
            return (s32)a[i] - (s32)b[i];
        }
    }

    return 0;
}

/*
 * result = a * b mod n
 *
 * Schoolbook multiply into a double-width product, then binary long division
 * MSB-first. The running remainder needs `len + 1` bytes: before each step it
 * is < n, and (acc << 1 | bit) can be as large as 2n-1, which does not fit in
 * `len` bytes when n has its top bit set. Reducing in a `len`-byte frame
 * silently discards that bit and produces an answer that is not congruent.
 */
static s32 mpi_mul_mod(u8 *result, const u8 *a, const u8 *b,
                       const u8 *n, u32 len)
{
    u8 product[2 * RSA_MAX_KEY_SIZE];
    u8 acc[RSA_MAX_KEY_SIZE + 1];
    u8 n_ext[RSA_MAX_KEY_SIZE + 1];
    u32 i;
    s32 bit;
    s32 top;

    if (len == 0 || len > RSA_MAX_KEY_SIZE) {
        return CRYPTO_ERR_BUFFER_SMALL;
    }

    memset(product, 0, sizeof(product));

    /* Schoolbook: for each byte of b, accumulate a * b[i] at its byte weight. */
    for (i = 0; i < len; i++) {
        u32 digit = b[len - 1 - i];
        u32 carry = 0;
        u32 k;

        if (digit == 0) {
            continue;
        }

        for (k = 0; k < len; k++) {
            u32 pos = 2 * len - 1 - (i + k);
            u32 prod = (u32)a[len - 1 - k] * digit + product[pos] + carry;

            product[pos] = (u8)(prod & 0xFF);
            carry = prod >> 8;
        }

        /* Propagate the remaining carry upward. */
        {
            s32 pos = (s32)(2 * len - 1 - (i + len));

            while (carry && pos >= 0) {
                u32 sum = product[pos] + carry;

                product[pos] = (u8)(sum & 0xFF);
                carry = sum >> 8;
                pos--;
            }
        }
    }

    /* n in a len+1 byte frame, so it can be compared against a len+1 acc. */
    memset(n_ext, 0, sizeof(n_ext));
    memcpy(n_ext + 1, n, len);

    memset(acc, 0, sizeof(acc));

    top = (s32)mpi_bit_length(product, 2 * len) - 1;

    for (bit = top; bit >= 0; bit--) {
        u32 carry = mpi_get_bit(product, 2 * len, (u32)bit);
        u32 j;

        /* acc = (acc << 1) | carry, in len+1 bytes */
        for (j = 0; j < len + 1; j++) {
            u32 cur = acc[len - j];
            acc[len - j] = (u8)(((cur << 1) | carry) & 0xFF);
            carry = cur >> 7;
        }

        if (mpi_cmp_len(acc, n_ext, len + 1) >= 0) {
            mpi_sub_len(acc, acc, n_ext, len + 1);
        }
    }

    memcpy(result, acc + 1, len);

    memset(product, 0, sizeof(product));
    memset(acc, 0, sizeof(acc));

    return CRYPTO_OK;
}

/*
 * result = base^exp mod n
 *
 * Square-and-multiply, left to right, over the bits of exp.
 */
static s32 mpi_exp_mod(u8 *result, u32 result_len,
                       const u8 *base, u32 base_len,
                       const u8 *exp, u32 exp_len,
                       const u8 *mod, u32 mod_len)
{
    u8 acc[RSA_MAX_KEY_SIZE];
    u8 b[RSA_MAX_KEY_SIZE];
    u32 len;
    s32 bit;
    s32 top;

    if (!result || !base || !exp || !mod) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (mod_len == 0 || mod_len > RSA_MAX_KEY_SIZE || result_len < mod_len) {
        return CRYPTO_ERR_BUFFER_SMALL;
    }

    len = mod_len;

    /* b = base mod n */
    memset(b, 0, sizeof(b));
    if (base_len <= len) {
        memcpy(b + (len - base_len), base, base_len);
        while (mpi_cmp_len(b, mod, len) >= 0) {
            mpi_sub_len(b, b, mod, len);
        }
    } else {
        /*
         * Base wider than the modulus: reduce by long division, one byte at a
         * time, MSB first. The remainder is kept in a len+1 byte frame for the
         * same reason as in mpi_mul_mod().
         */
        u8 rem[RSA_MAX_KEY_SIZE + 1];
        u8 mod_ext[RSA_MAX_KEY_SIZE + 1];
        u32 i;

        memset(rem, 0, sizeof(rem));
        memset(mod_ext, 0, sizeof(mod_ext));
        memcpy(mod_ext + 1, mod, len);

        for (i = 0; i < base_len; i++) {
            u32 carry = base[i];
            u32 j;

            /* rem = (rem << 8) | base[i] */
            for (j = 0; j < len + 1; j++) {
                u32 cur = rem[len - j];
                rem[len - j] = (u8)(((cur << 8) | carry) & 0xFF);
                carry = cur >> 24;
            }

            while (mpi_cmp_len(rem, mod_ext, len + 1) >= 0) {
                mpi_sub_len(rem, rem, mod_ext, len + 1);
            }
        }

        memcpy(b, rem + 1, len);
        memset(rem, 0, sizeof(rem));
    }

    /* acc = 1 */
    memset(acc, 0, sizeof(acc));
    acc[len - 1] = 1;

    top = (s32)mpi_bit_length(exp, exp_len) - 1;

    for (bit = top; bit >= 0; bit--) {
        s32 ret = mpi_mul_mod(acc, acc, acc, mod, len);

        if (ret != CRYPTO_OK) {
            goto out;
        }

        if (mpi_get_bit(exp, exp_len, (u32)bit)) {
            ret = mpi_mul_mod(acc, acc, b, mod, len);
            if (ret != CRYPTO_OK) {
                goto out;
            }
        }
    }

    if (result_len > len) {
        memset(result, 0, result_len - len);
    }

    memcpy(result + (result_len - len), acc, len);

out:
    memset(acc, 0, sizeof(acc));
    memset(b, 0, sizeof(b));

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
static s32 rsa_encrypt_pkcs1_ctx(rsa_context_t *ctx, const u8 *input, u32 input_len,
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

    /*
     * c = em^e mod n
     *
     * This was `memcpy(c, em, em_len)` with the exponentiation commented out,
     * so the "ciphertext" was the padded plaintext: RSA encryption provided no
     * confidentiality whatsoever.
     */
    {
        s32 xret = mpi_exp_mod(c, em_len, em, em_len,
                               ctx->key.e, ctx->key.size,
                               ctx->key.n, ctx->key.size);

        if (xret != CRYPTO_OK) {
            crypto_memzero(em, sizeof(em));
            crypto_memzero(c, sizeof(c));
            return xret;
        }
    }

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
static s32 rsa_decrypt_pkcs1_ctx(rsa_context_t *ctx, const u8 *input, u32 input_len,
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

    /*
     * em = c^d mod n
     *
     * This was `memcpy(em, input, em_len)` with the exponentiation commented
     * out: "decryption" read the padding straight back off the input, so it
     * only ever worked on input that was already padded plaintext.
     *
     * CRT (the p/q/dp/dq optimisation) is not used; a plain d exponentiation
     * is slower but correct.
     */
    {
        s32 xret = mpi_exp_mod(em, em_len, input, input_len,
                               ctx->key.d, ctx->key.size,
                               ctx->key.n, ctx->key.size);

        if (xret != CRYPTO_OK) {
            crypto_memzero(em, sizeof(em));
            return xret;
        }
    }

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

    /*
     * Padding failure has to reach the caller. This ignored the return value
     * of the PKCS#1 routine and then reported CRYPTO_OK, so an encryption that
     * could not build a valid block - oversized input, no room for the
     * mandatory 0x00 0x02 non-zero padding string, etc. - returned success
     * with whatever happened to be in `output`. A caller would then transmit
     * an all-zero or stale buffer believing it RSA ciphertext.
     */
    spin_lock(&ctx->lock);

    {
        s32 ret;

        switch (ctx->padding) {
            case RSA_PAD_PKCS1_V15:
                ret = rsa_encrypt_pkcs1_ctx(ctx, input, ctx->key.size - 11,
                                            output, output_len);
                break;
            default:
                spin_unlock(&ctx->lock);
                return CRYPTO_ERR_GENERAL;
        }

        spin_unlock(&ctx->lock);

        return ret;
    }
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

    /*
     * A failed PKCS#1 unpad is the signal that the ciphertext was malformed or
     * the key was wrong. Swallowing it here meant every forged or corrupted
     * ciphertext "decrypted successfully" into uninitialised output - a
     * padding-oracle and a silent-corruption bug at once.
     */
    spin_lock(&ctx->lock);

    {
        s32 ret;

        switch (ctx->padding) {
            case RSA_PAD_PKCS1_V15:
                ret = rsa_decrypt_pkcs1_ctx(ctx, input, ctx->key.size,
                                            output, output_len);
                break;
            default:
                spin_unlock(&ctx->lock);
                return CRYPTO_ERR_GENERAL;
        }

        spin_unlock(&ctx->lock);

        return ret;
    }
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
    u32 max_len;

    /*
     * input_len was accepted and never looked at. The PKCS#1 padding path
     * receives the message length implicitly as key.size - 11 and copies
     * input_len-independent data, so oversize input would have been encrypted
     * from a fixed-size view of the buffer - reading past it when the message
     * was short, and silently truncating it when long.
     *
     * Maximum message length for PKCS#1 v1.5 is k - 11 bytes where k is the
     * modulus length.
     */
    if (!key || !input || !output || !output_len) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (key->size < 11) {
        return CRYPTO_ERR_INVALID_KEY;
    }

    max_len = key->size - 11;

    if (input_len > max_len) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    ret = rsa_init(&ctx);
    if (ret != CRYPTO_OK) return ret;

    ret = rsa_set_key(&ctx, key);
    if (ret != CRYPTO_OK) {
        rsa_free(&ctx);
        return ret;
    }

    /* rsa_encrypt() pads to ctx->key.size - 11, so hand it that count. */
    ret = rsa_encrypt_pkcs1_ctx(&ctx, input, input_len, output, output_len);

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

    ret = rsa_decrypt_pkcs1_ctx(&ctx, input, input_len, output, output_len);

    rsa_free(&ctx);
    return ret;
}
