/*
 * NEXUS OS - Security Framework
 * security/crypto/sha.c
 *
 * SHA Hashing Implementation
 * SHA-256 and SHA-512 secure hash algorithms
 */

#include "crypto.h"

/*===========================================================================*/
/*                         SHA CONFIGURATION                                 */
/*===========================================================================*/

#define SHA_DEBUG               0

/*===========================================================================*/
/*                         SHA-256 CONSTANTS                                 */
/*===========================================================================*/

/**
 * sha256_k - SHA-256 round constants
 *
 * First 32 bits of the fractional parts of the cube roots
 * of the first 64 primes 2..311.
 */
static const u32 sha256_k[64] = {
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
    0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
    0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
    0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
    0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
    0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7,
    0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
    0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
    0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
    0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3,
    0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5,
    0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
    0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
    0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2
};

/**
 * sha256_h0 - SHA-256 initial hash values
 *
 * First 32 bits of the fractional parts of the square roots
 * of the first 8 primes 2..19.
 */
static const u32 sha256_h0[8] = {
    0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
    0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
};

/*===========================================================================*/
/*                         SHA-512 CONSTANTS                                 */
/*===========================================================================*/

/**
 * sha512_k - SHA-512 round constants
 *
 * First 64 bits of the fractional parts of the cube roots
 * of the first 80 primes 2..409.
 */
static const u64 sha512_k[80] = {
    0x428A2F98D728AE22ULL, 0x7137449123EF65CDULL, 0xB5C0FBCFEC4D3B2FULL, 0xE9B5DBA58189DBBCULL,
    0x3956C25BF348B538ULL, 0x59F111F1B605D019ULL, 0x923F82A4AF194F9BULL, 0xAB1C5ED5DA6D8118ULL,
    0xD807AA98A3030242ULL, 0x12835B0145706FBEULL, 0x243185BE4EE4B28CULL, 0x550C7DC3D5FFB4E2ULL,
    0x72BE5D74F27B896FULL, 0x80DEB1FE3B1696B1ULL, 0x9BDC06A725C71235ULL, 0xC19BF174CF692694ULL,
    0xE49B69C19EF14AD2ULL, 0xEFBE4786384F25E3ULL, 0x0FC19DC68B8CD5B5ULL, 0x240CA1CC77AC9C65ULL,
    0x2DE92C6F592B0275ULL, 0x4A7484AA6EA6E483ULL, 0x5CB0A9DCBD41FBD4ULL, 0x76F988DA831153B5ULL,
    0x983E5152EE66DFABULL, 0xA831C66D2DB43210ULL, 0xB00327C898FB213FULL, 0xBF597FC7BEEF0EE4ULL,
    0xC6E00BF33DA88FC2ULL, 0xD5A79147930AA725ULL, 0x06CA6351E003826FULL, 0x142929670A0E6E70ULL,
    0x27B70A8546D22FFCULL, 0x2E1B21385C26C926ULL, 0x4D2C6DFC5AC42AEDULL, 0x53380D139D95B3DFULL,
    0x650A73548BAF63DEULL, 0x766A0ABB3C77B2A8ULL, 0x81C2C92E47EDAEE6ULL, 0x92722C851482353BULL,
    0xA2BFE8A14CF10364ULL, 0xA81A664BBC423001ULL, 0xC24B8B70D0F89791ULL, 0xC76C51A30654BE30ULL,
    0xD192E819D6EF5218ULL, 0xD69906245565A910ULL, 0xF40E35855771202AULL, 0x106AA07032BBD1B8ULL,
    0x19A4C116B8D2D0C8ULL, 0x1E376C085141AB53ULL, 0x2748774CDF8EEB99ULL, 0x34B0BCB5E19B48A8ULL,
    0x391C0CB3C5C95A63ULL, 0x4ED8AA4AE3418ACBULL, 0x5B9CCA4F7763E373ULL, 0x682E6FF3D6B2B8A3ULL,
    0x748F82EE5DEFB2FCULL, 0x78A5636F43172F60ULL, 0x84C87814A1F0AB72ULL, 0x8CC702081A6439ECULL,
    0x90BEFFFA23631E28ULL, 0xA4506CEBDE82BDE9ULL, 0xBEF9A3F7B2C67915ULL, 0xC67178F2E372532BULL,
    0xCA273ECEEA26619CULL, 0xD186B8C721C0C207ULL, 0xEADA7DD6CDE0EB1EULL, 0xF57D4F7FEE6ED178ULL,
    0x06F067AA72176FBAULL, 0x0A637DC5A2C898A6ULL, 0x113F9804BEF90DAEULL, 0x1B710B35131C471BULL,
    0x28DB77F523047D84ULL, 0x32CAAB7B40C72493ULL, 0x3C9EBE0A15C9BEBCULL, 0x431D67C49C100D4CULL,
    0x4CC5D4BECB3E42B6ULL, 0x597F299CFC657E2AULL, 0x5FCB6FAB3AD6FAECULL, 0x6C44198C4A475817ULL
};

/**
 * sha512_h0 - SHA-512 initial hash values
 *
 * First 64 bits of the fractional parts of the square roots
 * of the first 8 primes 2..19.
 */
static const u64 sha512_h0[8] = {
    0x6A09E667F3BCC908ULL, 0xBB67AE8584CAA73BULL, 0x3C6EF372FE94F82BULL, 0xA54FF53A5F1D36F1ULL,
    0x510E527FADE682D1ULL, 0x9B05688C2B3E6C1FULL, 0x1F83D9ABFB41BD6BULL, 0x5BE0CD19137E2179ULL
};

/*===========================================================================*/
/*                         SHA-256 CORE FUNCTIONS                            */
/*===========================================================================*/

/**
 * sha256_rotr - Rotate right for 32-bit values
 * @x: Value to rotate
 * @n: Number of bits to rotate
 *
 * Returns: Rotated value
 */
static inline u32 sha256_rotr(u32 x, u32 n)
{
    return (x >> n) | (x << (32 - n));
}

/**
 * sha256_ch - SHA-256 choice function
 * @x, @y, @z: Input values
 *
 * Returns: ch(x, y, z) = (x AND y) XOR (NOT x AND z)
 */
static inline u32 sha256_ch(u32 x, u32 y, u32 z)
{
    return (x & y) ^ (~x & z);
}

/**
 * sha256_maj - SHA-256 majority function
 * @x, @y, @z: Input values
 *
 * Returns: maj(x, y, z) = (x AND y) XOR (x AND z) XOR (y AND z)
 */
static inline u32 sha256_maj(u32 x, u32 y, u32 z)
{
    return (x & y) ^ (x & z) ^ (y & z);
}

/**
 * sha256_sigma0 - SHA-256 Σ0 function
 * @x: Input value
 *
 * Returns: Σ0(x) = ROTR(2) XOR ROTR(13) XOR ROTR(22)
 */
static inline u32 sha256_sigma0(u32 x)
{
    return sha256_rotr(x, 2) ^ sha256_rotr(x, 13) ^ sha256_rotr(x, 22);
}

/**
 * sha256_sigma1 - SHA-256 Σ1 function
 * @x: Input value
 *
 * Returns: Σ1(x) = ROTR(6) XOR ROTR(11) XOR ROTR(25)
 */
static inline u32 sha256_sigma1(u32 x)
{
    return sha256_rotr(x, 6) ^ sha256_rotr(x, 11) ^ sha256_rotr(x, 25);
}

/**
 * sha256_gamma0 - SHA-256 σ0 function
 * @x: Input value
 *
 * Returns: σ0(x) = ROTR(7) XOR ROTR(18) XOR SHR(3)
 */
static inline u32 sha256_gamma0(u32 x)
{
    return sha256_rotr(x, 7) ^ sha256_rotr(x, 18) ^ (x >> 3);
}

/**
 * sha256_gamma1 - SHA-256 σ1 function
 * @x: Input value
 *
 * Returns: σ1(x) = ROTR(17) XOR ROTR(19) XOR SHR(10)
 */
static inline u32 sha256_gamma1(u32 x)
{
    return sha256_rotr(x, 17) ^ sha256_rotr(x, 19) ^ (x >> 10);
}

/**
 * sha256_transform - SHA-256 block transformation
 * @ctx: SHA context
 * @block: 64-byte input block
 *
 * Processes a single 512-bit block through the SHA-256
 * compression function.
 */
static void sha256_transform(sha_context_t *ctx, const u8 *block)
{
    u32 w[64];
    u32 a, b, c, d, e, f, g, h;
    u32 t1, t2;
    u32 i;

    /* Prepare message schedule */
    for (i = 0; i < 16; i++) {
        w[i] = ((u32)block[i * 4] << 24) |
               ((u32)block[i * 4 + 1] << 16) |
               ((u32)block[i * 4 + 2] << 8) |
               ((u32)block[i * 4 + 3]);
    }

    for (i = 16; i < 64; i++) {
        w[i] = sha256_gamma1(w[i - 2]) + w[i - 7] +
               sha256_gamma0(w[i - 15]) + w[i - 16];
    }

    /* Initialize working variables */
    a = ctx->state_sha256[0];
    b = ctx->state_sha256[1];
    c = ctx->state_sha256[2];
    d = ctx->state_sha256[3];
    e = ctx->state_sha256[4];
    f = ctx->state_sha256[5];
    g = ctx->state_sha256[6];
    h = ctx->state_sha256[7];

    /* Main compression loop */
    for (i = 0; i < 64; i++) {
        t1 = h + sha256_sigma1(e) + sha256_ch(e, f, g) + sha256_k[i] + w[i];
        t2 = sha256_sigma0(a) + sha256_maj(a, b, c);

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    /* Add compressed chunk to current hash value */
    ctx->state_sha256[0] += a;
    ctx->state_sha256[1] += b;
    ctx->state_sha256[2] += c;
    ctx->state_sha256[3] += d;
    ctx->state_sha256[4] += e;
    ctx->state_sha256[5] += f;
    ctx->state_sha256[6] += g;
    ctx->state_sha256[7] += h;
}

/*===========================================================================*/
/*                         SHA-512 CORE FUNCTIONS                            */
/*===========================================================================*/

/**
 * sha512_rotr - Rotate right for 64-bit values
 * @x: Value to rotate
 * @n: Number of bits to rotate
 *
 * Returns: Rotated value
 */
static inline u64 sha512_rotr(u64 x, u64 n)
{
    return (x >> n) | (x << (64 - n));
}

/**
 * sha512_ch - SHA-512 choice function
 * @x, @y, @z: Input values
 *
 * Returns: ch(x, y, z) = (x AND y) XOR (NOT x AND z)
 */
static inline u64 sha512_ch(u64 x, u64 y, u64 z)
{
    return (x & y) ^ (~x & z);
}

/**
 * sha512_maj - SHA-512 majority function
 * @x, @y, @z: Input values
 *
 * Returns: maj(x, y, z) = (x AND y) XOR (x AND z) XOR (y AND z)
 */
static inline u64 sha512_maj(u64 x, u64 y, u64 z)
{
    return (x & y) ^ (x & z) ^ (y & z);
}

/**
 * sha512_sigma0 - SHA-512 Σ0 function
 * @x: Input value
 *
 * Returns: Σ0(x) = ROTR(28) XOR ROTR(34) XOR ROTR(39)
 */
static inline u64 sha512_sigma0(u64 x)
{
    return sha512_rotr(x, 28) ^ sha512_rotr(x, 34) ^ sha512_rotr(x, 39);
}

/**
 * sha512_sigma1 - SHA-512 Σ1 function
 * @x: Input value
 *
 * Returns: Σ1(x) = ROTR(14) XOR ROTR(18) XOR ROTR(41)
 */
static inline u64 sha512_sigma1(u64 x)
{
    return sha512_rotr(x, 14) ^ sha512_rotr(x, 18) ^ sha512_rotr(x, 41);
}

/**
 * sha512_gamma0 - SHA-512 σ0 function
 * @x: Input value
 *
 * Returns: σ0(x) = ROTR(1) XOR ROTR(8) XOR SHR(7)
 */
static inline u64 sha512_gamma0(u64 x)
{
    return sha512_rotr(x, 1) ^ sha512_rotr(x, 8) ^ (x >> 7);
}

/**
 * sha512_gamma1 - SHA-512 σ1 function
 * @x: Input value
 *
 * Returns: σ1(x) = ROTR(19) XOR ROTR(61) XOR SHR(6)
 */
static inline u64 sha512_gamma1(u64 x)
{
    return sha512_rotr(x, 19) ^ sha512_rotr(x, 61) ^ (x >> 6);
}

/**
 * sha512_transform - SHA-512 block transformation
 * @ctx: SHA context
 * @block: 128-byte input block
 *
 * Processes a single 1024-bit block through the SHA-512
 * compression function.
 */
static void sha512_transform(sha_context_t *ctx, const u8 *block)
{
    u64 w[80];
    u64 a, b, c, d, e, f, g, h;
    u64 t1, t2;
    u32 i;

    /* Prepare message schedule */
    for (i = 0; i < 16; i++) {
        w[i] = ((u64)block[i * 8] << 56) |
               ((u64)block[i * 8 + 1] << 48) |
               ((u64)block[i * 8 + 2] << 40) |
               ((u64)block[i * 8 + 3] << 32) |
               ((u64)block[i * 8 + 4] << 24) |
               ((u64)block[i * 8 + 5] << 16) |
               ((u64)block[i * 8 + 6] << 8) |
               ((u64)block[i * 8 + 7]);
    }

    for (i = 16; i < 80; i++) {
        w[i] = sha512_gamma1(w[i - 2]) + w[i - 7] +
               sha512_gamma0(w[i - 15]) + w[i - 16];
    }

    /* Initialize working variables */
    a = ctx->state_sha512[0];
    b = ctx->state_sha512[1];
    c = ctx->state_sha512[2];
    d = ctx->state_sha512[3];
    e = ctx->state_sha512[4];
    f = ctx->state_sha512[5];
    g = ctx->state_sha512[6];
    h = ctx->state_sha512[7];

    /* Main compression loop */
    for (i = 0; i < 80; i++) {
        t1 = h + sha512_sigma1(e) + sha512_ch(e, f, g) + sha512_k[i] + w[i];
        t2 = sha512_sigma0(a) + sha512_maj(a, b, c);

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    /* Add compressed chunk to current hash value */
    ctx->state_sha512[0] += a;
    ctx->state_sha512[1] += b;
    ctx->state_sha512[2] += c;
    ctx->state_sha512[3] += d;
    ctx->state_sha512[4] += e;
    ctx->state_sha512[5] += f;
    ctx->state_sha512[6] += g;
    ctx->state_sha512[7] += h;
}

/*===========================================================================*/
/*                         SHA-256 IMPLEMENTATION                            */
/*===========================================================================*/

/**
 * sha256_init - Initialize SHA-256 context
 * @ctx: SHA context to initialize
 *
 * Initializes a SHA context for SHA-256 hashing.
 *
 * Returns: OK on success, error code on failure
 */
s32 sha256_init(sha_context_t *ctx)
{
    if (!ctx) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock_init_named(&ctx->lock, "sha256_context");
    ctx->magic = CRYPTO_SHA_MAGIC;
    ctx->alg = CRYPTO_ALG_SHA256;
    ctx->block_size = SHA256_BLOCK_SIZE;
    ctx->digest_size = SHA256_DIGEST_SIZE;
    ctx->total[0] = 0;
    ctx->total[1] = 0;
    ctx->buf_len = 0;

    /* Initialize hash values */
    memcpy(ctx->state_sha256, sha256_h0, sizeof(sha256_h0));

    crypto_memzero(ctx->buffer, sizeof(ctx->buffer));

    pr_debug("SHA256: Context initialized at %p\n", ctx);

    return CRYPTO_OK;
}

/**
 * sha256_starts - Start a new SHA-256 hash
 * @ctx: SHA context
 *
 * Resets the context and starts a new hash computation.
 *
 * Returns: OK on success, error code on failure
 */
s32 sha256_starts(sha_context_t *ctx)
{
    if (!ctx || ctx->magic != CRYPTO_SHA_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock(&ctx->lock);

    ctx->total[0] = 0;
    ctx->total[1] = 0;
    ctx->buf_len = 0;

    memcpy(ctx->state_sha256, sha256_h0, sizeof(sha256_h0));
    crypto_memzero(ctx->buffer, sizeof(ctx->buffer));

    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/**
 * sha256_update - Update SHA-256 hash with data
 * @ctx: SHA context
 * @input: Input data
 * @length: Data length
 *
 * Processes input data and updates the hash state.
 * Can be called multiple times.
 *
 * Returns: OK on success, error code on failure
 */
s32 sha256_update(sha_context_t *ctx, const u8 *input, u32 length)
{
    u32 fill;
    u32 left;

    if (!ctx || ctx->magic != CRYPTO_SHA_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (!input || length == 0) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock(&ctx->lock);

    /* Update byte count */
    ctx->total[0] += length;
    if (ctx->total[0] < length) {
        ctx->total[1]++;
    }

    left = ctx->buf_len;
    fill = SHA256_BLOCK_SIZE - left;

    /* Process any data in the buffer */
    if (left > 0 && length >= fill) {
        memcpy(ctx->buffer + left, input, fill);
        sha256_transform(ctx, ctx->buffer);
        input += fill;
        length -= fill;
        left = 0;
    }

    /* Process full blocks directly from input */
    while (length >= SHA256_BLOCK_SIZE) {
        sha256_transform(ctx, input);
        input += SHA256_BLOCK_SIZE;
        length -= SHA256_BLOCK_SIZE;
    }

    /* Store remaining data in buffer */
    if (length > 0) {
        memcpy(ctx->buffer + left, input, length);
        ctx->buf_len = left + length;
    }

    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/**
 * sha256_finish - Finish SHA-256 hash computation
 * @ctx: SHA context
 * @output: Output buffer (32 bytes)
 *
 * Finalizes the hash computation and produces the digest.
 *
 * Returns: OK on success, error code on failure
 */
s32 sha256_finish(sha_context_t *ctx, u8 *output)
{
    u32 last;
    u32 padn;
    u32 msglen;
    u8 msglen_bytes[8];
    u32 i;

    if (!ctx || ctx->magic != CRYPTO_SHA_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (!output) {
        return CRYPTO_ERR_INVALID_OUTPUT;
    }

    spin_lock(&ctx->lock);

    /* Calculate message length in bits */
    msglen = ctx->buf_len;
    msglen_bytes[0] = (ctx->total[1] >> 56) & 0xFF;
    msglen_bytes[1] = (ctx->total[1] >> 48) & 0xFF;
    msglen_bytes[2] = (ctx->total[1] >> 40) & 0xFF;
    msglen_bytes[3] = (ctx->total[1] >> 32) & 0xFF;
    msglen_bytes[4] = (ctx->total[0] >> 24) & 0xFF;
    msglen_bytes[5] = (ctx->total[0] >> 16) & 0xFF;
    msglen_bytes[6] = (ctx->total[0] >> 8) & 0xFF;
    msglen_bytes[7] = ctx->total[0] & 0xFF;

    /* Add padding */
    last = ctx->buf_len;
    ctx->buffer[last++] = 0x80;

    if (last > SHA256_BLOCK_SIZE - 8) {
        /* Need to pad and process this block */
        padn = SHA256_BLOCK_SIZE - last;
        crypto_memzero(ctx->buffer + last, padn);
        sha256_transform(ctx, ctx->buffer);
        last = 0;
    }

    /* Pad to 56 bytes (448 bits) */
    padn = SHA256_BLOCK_SIZE - last - 8;
    crypto_memzero(ctx->buffer + last, padn);

    /* Append length */
    memcpy(ctx->buffer + SHA256_BLOCK_SIZE - 8, msglen_bytes, 8);

    /* Final transform */
    sha256_transform(ctx, ctx->buffer);

    /* Output hash in big-endian format */
    for (i = 0; i < 8; i++) {
        output[i * 4] = (ctx->state_sha256[i] >> 24) & 0xFF;
        output[i * 4 + 1] = (ctx->state_sha256[i] >> 16) & 0xFF;
        output[i * 4 + 2] = (ctx->state_sha256[i] >> 8) & 0xFF;
        output[i * 4 + 3] = ctx->state_sha256[i] & 0xFF;
    }

    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/**
 * sha_free - Free SHA context resources
 * @ctx: SHA context to free
 *
 * Securely clears SHA context resources.
 */
void sha_free(sha_context_t *ctx)
{
    if (!ctx || ctx->magic != CRYPTO_SHA_MAGIC) {
        return;
    }

    spin_lock(&ctx->lock);

    crypto_memzero(ctx->buffer, sizeof(ctx->buffer));
    crypto_memzero(ctx->state_sha256, sizeof(ctx->state_sha256));

    ctx->magic = 0;
    ctx->alg = CRYPTO_ALG_NONE;
    ctx->buf_len = 0;

    spin_unlock(&ctx->lock);

    pr_debug("SHA: Context freed at %p\n", ctx);
}

/**
 * sha512_init - Initialize SHA-512 context
 * @ctx: SHA context to initialize
 *
 * Initializes a SHA context for SHA-512 hashing.
 *
 * Returns: OK on success, error code on failure
 */
s32 sha512_init(sha_context_t *ctx)
{
    if (!ctx) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock_init_named(&ctx->lock, "sha512_context");
    ctx->magic = CRYPTO_SHA_MAGIC;
    ctx->alg = CRYPTO_ALG_SHA512;
    ctx->block_size = SHA512_BLOCK_SIZE;
    ctx->digest_size = SHA512_DIGEST_SIZE;
    ctx->total[0] = 0;
    ctx->total[1] = 0;
    ctx->buf_len = 0;

    /* Initialize hash values */
    memcpy(ctx->state_sha512, sha512_h0, sizeof(sha512_h0));

    crypto_memzero(ctx->buffer, sizeof(ctx->buffer));

    pr_debug("SHA512: Context initialized at %p\n", ctx);

    return CRYPTO_OK;
}

/**
 * sha512_update - Update SHA-512 hash with data
 * @ctx: SHA context
 * @input: Input data
 * @length: Data length
 *
 * Processes input data and updates the hash state.
 *
 * Returns: OK on success, error code on failure
 */
static s32 sha512_update(sha_context_t *ctx, const u8 *input, u32 length)
{
    u32 fill;
    u32 left;

    if (!ctx || ctx->magic != CRYPTO_SHA_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (!input || length == 0) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock(&ctx->lock);

    ctx->total[0] += length;
    if (ctx->total[0] < length) {
        ctx->total[1]++;
    }

    left = ctx->buf_len;
    fill = SHA512_BLOCK_SIZE - left;

    if (left > 0 && length >= fill) {
        memcpy(ctx->buffer + left, input, fill);
        sha512_transform(ctx, ctx->buffer);
        input += fill;
        length -= fill;
        left = 0;
    }

    while (length >= SHA512_BLOCK_SIZE) {
        sha512_transform(ctx, input);
        input += SHA512_BLOCK_SIZE;
        length -= SHA512_BLOCK_SIZE;
    }

    if (length > 0) {
        memcpy(ctx->buffer + left, input, length);
        ctx->buf_len = left + length;
    }

    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/**
 * sha512_finish - Finish SHA-512 hash computation
 * @ctx: SHA context
 * @output: Output buffer (64 bytes)
 *
 * Finalizes the hash computation and produces the digest.
 *
 * Returns: OK on success, error code on failure
 */
static s32 sha512_finish(sha_context_t *ctx, u8 *output)
{
    u32 last;
    u32 padn;
    u8 msglen_bytes[16];
    u32 i;

    if (!ctx || ctx->magic != CRYPTO_SHA_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (!output) {
        return CRYPTO_ERR_INVALID_OUTPUT;
    }

    spin_lock(&ctx->lock);

    /* Calculate message length in bits (128-bit) */
    u64 total_bits = (ctx->total[0] + ((u64)ctx->total[1] << 32)) * 8;
    for (i = 0; i < 8; i++) {
        msglen_bytes[i] = (total_bits >> (56 - i * 8)) & 0xFF;
        msglen_bytes[8 + i] = 0;  /* Upper 64 bits are 0 for reasonable sizes */
    }

    last = ctx->buf_len;
    ctx->buffer[last++] = 0x80;

    if (last > SHA512_BLOCK_SIZE - 16) {
        padn = SHA512_BLOCK_SIZE - last;
        crypto_memzero(ctx->buffer + last, padn);
        sha512_transform(ctx, ctx->buffer);
        last = 0;
    }

    padn = SHA512_BLOCK_SIZE - last - 16;
    crypto_memzero(ctx->buffer + last, padn);

    memcpy(ctx->buffer + SHA512_BLOCK_SIZE - 16, msglen_bytes, 16);

    sha512_transform(ctx, ctx->buffer);

    /* Output hash in big-endian format */
    for (i = 0; i < 8; i++) {
        output[i * 8] = (ctx->state_sha512[i] >> 56) & 0xFF;
        output[i * 8 + 1] = (ctx->state_sha512[i] >> 48) & 0xFF;
        output[i * 8 + 2] = (ctx->state_sha512[i] >> 40) & 0xFF;
        output[i * 8 + 3] = (ctx->state_sha512[i] >> 32) & 0xFF;
        output[i * 8 + 4] = (ctx->state_sha512[i] >> 24) & 0xFF;
        output[i * 8 + 5] = (ctx->state_sha512[i] >> 16) & 0xFF;
        output[i * 8 + 6] = (ctx->state_sha512[i] >> 8) & 0xFF;
        output[i * 8 + 7] = ctx->state_sha512[i] & 0xFF;
    }

    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/*===========================================================================*/
/*                         SHA ONE-SHOT OPERATIONS                           */
/*===========================================================================*/

/**
 * sha256 - One-shot SHA-256 hash
 * @input: Input data
 * @length: Data length
 * @output: Output digest (32 bytes)
 *
 * Computes SHA-256 hash in a single call.
 *
 * Returns: OK on success, error code on failure
 */
s32 sha256(const u8 *input, u32 length, u8 *output)
{
    sha_context_t ctx;
    s32 ret;

    if (!input || !output) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    ret = sha256_init(&ctx);
    if (ret != CRYPTO_OK) {
        return ret;
    }

    ret = sha256_update(&ctx, input, length);
    if (ret != CRYPTO_OK) {
        sha_free(&ctx);
        return ret;
    }

    ret = sha256_finish(&ctx, output);

    sha_free(&ctx);
    return ret;
}

/**
 * sha512 - One-shot SHA-512 hash
 * @input: Input data
 * @length: Data length
 * @output: Output digest (64 bytes)
 *
 * Computes SHA-512 hash in a single call.
 *
 * Returns: OK on success, error code on failure
 */
s32 sha512(const u8 *input, u32 length, u8 *output)
{
    sha_context_t ctx;
    s32 ret;

    if (!input || !output) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    ret = sha512_init(&ctx);
    if (ret != CRYPTO_OK) {
        return ret;
    }

    ret = sha512_update(&ctx, input, length);
    if (ret != CRYPTO_OK) {
        sha_free(&ctx);
        return ret;
    }

    ret = sha512_finish(&ctx, output);

    sha_free(&ctx);
    return ret;
}

/*===========================================================================*/
/*                         HMAC IMPLEMENTATION                               */
/*===========================================================================*/

/**
 * hmac_sha256 - HMAC-SHA256 computation
 * @key: HMAC key
 * @key_len: Key length
 * @input: Input data
 * @input_len: Input length
 * @output: Output digest (32 bytes)
 *
 * Computes HMAC-SHA256: HMAC(K, m) = H((K' XOR opad) || H((K' XOR ipad) || m))
 *
 * Returns: OK on success, error code on failure
 */
s32 hmac_sha256(const u8 *key, u32 key_len, const u8 *input, u32 input_len, u8 *output)
{
    sha_context_t ctx;
    u8 k_pad[SHA256_BLOCK_SIZE];
    u8 inner_hash[SHA256_DIGEST_SIZE];
    u32 i;

    if (!key || !input || !output) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    /* If key is longer than block size, hash it */
    if (key_len > SHA256_BLOCK_SIZE) {
        sha256(key, key_len, k_pad);
        crypto_memzero(k_pad + SHA256_DIGEST_SIZE, SHA256_BLOCK_SIZE - SHA256_DIGEST_SIZE);
    } else {
        memcpy(k_pad, key, key_len);
        crypto_memzero(k_pad + key_len, SHA256_BLOCK_SIZE - key_len);
    }

    /* Inner padding: XOR with 0x36 */
    for (i = 0; i < SHA256_BLOCK_SIZE; i++) {
        k_pad[i] ^= 0x36;
    }

    /* Inner hash: H((K XOR ipad) || message) */
    sha256_init(&ctx);
    sha256_update(&ctx, k_pad, SHA256_BLOCK_SIZE);
    sha256_update(&ctx, input, input_len);
    sha256_finish(&ctx, inner_hash);
    sha_free(&ctx);

    /* Outer padding: XOR with 0x5c */
    for (i = 0; i < SHA256_BLOCK_SIZE; i++) {
        k_pad[i] ^= (0x36 ^ 0x5c);
    }

    /* Outer hash: H((K XOR opad) || inner_hash) */
    sha256_init(&ctx);
    sha256_update(&ctx, k_pad, SHA256_BLOCK_SIZE);
    sha256_update(&ctx, inner_hash, SHA256_DIGEST_SIZE);
    sha256_finish(&ctx, output);
    sha_free(&ctx);

    crypto_memzero(k_pad, sizeof(k_pad));
    crypto_memzero(inner_hash, sizeof(inner_hash));

    return CRYPTO_OK;
}

/**
 * hmac_sha512 - HMAC-SHA512 computation
 * @key: HMAC key
 * @key_len: Key length
 * @input: Input data
 * @input_len: Input length
 * @output: Output digest (64 bytes)
 *
 * Computes HMAC-SHA512.
 *
 * Returns: OK on success, error code on failure
 */
s32 hmac_sha512(const u8 *key, u32 key_len, const u8 *input, u32 input_len, u8 *output)
{
    sha_context_t ctx;
    u8 k_pad[SHA512_BLOCK_SIZE];
    u8 inner_hash[SHA512_DIGEST_SIZE];
    u32 i;

    if (!key || !input || !output) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    /* If key is longer than block size, hash it */
    if (key_len > SHA512_BLOCK_SIZE) {
        sha512(key, key_len, k_pad);
        crypto_memzero(k_pad + SHA512_DIGEST_SIZE, SHA512_BLOCK_SIZE - SHA512_DIGEST_SIZE);
    } else {
        memcpy(k_pad, key, key_len);
        crypto_memzero(k_pad + key_len, SHA512_BLOCK_SIZE - key_len);
    }

    /* Inner padding: XOR with 0x36 */
    for (i = 0; i < SHA512_BLOCK_SIZE; i++) {
        k_pad[i] ^= 0x36;
    }

    /* Inner hash */
    sha512_init(&ctx);
    sha512_update(&ctx, k_pad, SHA512_BLOCK_SIZE);
    sha512_update(&ctx, input, input_len);
    sha512_finish(&ctx, inner_hash);
    sha_free(&ctx);

    /* Outer padding: XOR with 0x5c */
    for (i = 0; i < SHA512_BLOCK_SIZE; i++) {
        k_pad[i] ^= (0x36 ^ 0x5c);
    }

    /* Outer hash */
    sha512_init(&ctx);
    sha512_update(&ctx, k_pad, SHA512_BLOCK_SIZE);
    sha512_update(&ctx, inner_hash, SHA512_DIGEST_SIZE);
    sha512_finish(&ctx, output);
    sha_free(&ctx);

    crypto_memzero(k_pad, sizeof(k_pad));
    crypto_memzero(inner_hash, sizeof(inner_hash));

    return CRYPTO_OK;
}

/*===========================================================================*/
/*                         PBKDF2 IMPLEMENTATION                             */
/*===========================================================================*/

/**
 * pbkdf2_sha256 - PBKDF2 key derivation with SHA-256
 * @password: Password
 * @pass_len: Password length
 * @salt: Salt
 * @salt_len: Salt length
 * @iterations: Iteration count
 * @output: Output key
 * @output_len: Output length
 *
 * Implements PBKDF2-HMAC-SHA256 key derivation function.
 * DK = T1 || T2 || ... || Tdklen/hlen
 * Ti = F(Password, Salt, c, i)
 * F = U1 XOR U2 XOR ... XOR Uc
 * U1 = PRF(Password, Salt || INT(i))
 * Uj = PRF(Password, Uj-1)
 *
 * Returns: OK on success, error code on failure
 */
s32 pbkdf2_sha256(const u8 *password, u32 pass_len, const u8 *salt, u32 salt_len,
                  u32 iterations, u8 *output, u32 output_len)
{
    u8 block[SHA256_DIGEST_SIZE];
    u8 hash[SHA256_DIGEST_SIZE];
    u8 salt_counter[4];
    u32 block_count;
    u32 i, j;
    u32 blocks_needed;

    if (!password || !salt || !output) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (output_len == 0 || iterations == 0) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    blocks_needed = (output_len + SHA256_DIGEST_SIZE - 1) / SHA256_DIGEST_SIZE;

    for (block_count = 1; block_count <= blocks_needed; block_count++) {
        /* INT(i) - block counter as big-endian 32-bit */
        salt_counter[0] = (block_count >> 24) & 0xFF;
        salt_counter[1] = (block_count >> 16) & 0xFF;
        salt_counter[2] = (block_count >> 8) & 0xFF;
        salt_counter[3] = block_count & 0xFF;

        /* U1 = PRF(Password, Salt || INT(i)) */
        hmac_sha256(password, pass_len, salt, salt_len, block);
        hmac_sha256(password, pass_len, salt_counter, 4, block);

        memcpy(hash, block, SHA256_DIGEST_SIZE);

        /* U2 ... Uc */
        for (i = 2; i <= iterations; i++) {
            hmac_sha256(password, pass_len, block, SHA256_DIGEST_SIZE, block);

            for (j = 0; j < SHA256_DIGEST_SIZE; j++) {
                hash[j] ^= block[j];
            }
        }

        /* Copy to output */
        for (i = 0; i < SHA256_DIGEST_SIZE && output_len > 0; i++, output_len--) {
            *output++ = hash[i];
        }
    }

    crypto_memzero(block, sizeof(block));
    crypto_memzero(hash, sizeof(hash));

    return CRYPTO_OK;
}
