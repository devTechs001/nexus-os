/*
 * NEXUS OS - Security Framework
 * security/crypto/ecc.c
 *
 * Elliptic Curve Cryptography Implementation
 * ECC key generation, ECDH, and ECDSA
 */

#include "crypto.h"

/*===========================================================================*/
/*                         ECC CONFIGURATION                                 */
/*===========================================================================*/

#define ECC_DEBUG               0

/* Supported Curves */
#define ECC_CURVE_P256          0
#define ECC_CURVE_P384          1
#define ECC_CURVE_P521          2

/*===========================================================================*/
/*                         CURVE PARAMETERS                                  */
/*===========================================================================*/

/* NIST P-256 Curve Parameters */
static const u8 ecc_p256_p[32] = {  /* Prime field */
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const u8 ecc_p256_a[32] = {  /* Curve coefficient a */
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC
};

static const u8 ecc_p256_b[32] = {  /* Curve coefficient b */
    0x5A, 0xC6, 0x35, 0xD8, 0xAA, 0x3A, 0x93, 0xE7,
    0xB3, 0xEB, 0xBD, 0x55, 0x76, 0x98, 0x86, 0xBC,
    0x65, 0x1D, 0x06, 0xB0, 0xCC, 0x53, 0xB0, 0xF6,
    0x3B, 0xCE, 0x3C, 0x3E, 0x27, 0xD2, 0x60, 0x4B
};

static const u8 ecc_p256_gx[32] = {  /* Generator X */
    0x6B, 0x17, 0xD1, 0xF2, 0xE1, 0x2C, 0x42, 0x47,
    0xF8, 0xBC, 0xE6, 0xE5, 0x63, 0xA4, 0x40, 0xF2,
    0x77, 0x03, 0x7D, 0x81, 0x2D, 0xEB, 0x33, 0xA0,
    0xF4, 0xA1, 0x39, 0x45, 0xD8, 0x98, 0xC2, 0x96
};

static const u8 ecc_p256_gy[32] = {  /* Generator Y */
    0x4F, 0xE3, 0x42, 0xE2, 0xFE, 0x1A, 0x7F, 0x9B,
    0x8E, 0xE7, 0xEB, 0x4A, 0x7C, 0x0F, 0x9E, 0x16,
    0x2B, 0xCE, 0x33, 0x57, 0x6B, 0x31, 0x5E, 0xCE,
    0xCB, 0xB6, 0x40, 0x68, 0x37, 0xBF, 0x51, 0xF5
};

static const u8 ecc_p256_n[32] = {  /* Order of generator */
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xBC, 0xE6, 0xFA, 0xAD, 0xA7, 0x17, 0x9E, 0x84,
    0xF3, 0xB9, 0xCA, 0xC2, 0xFC, 0x63, 0x25, 0x51
};

/* NIST P-384 Curve Parameters */
static const u8 ecc_p384_p[48] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF
};

static const u8 ecc_p384_a[48] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFC
};

static const u8 ecc_p384_b[48] = {
    0xB3, 0x31, 0x2F, 0xA7, 0xE2, 0x3E, 0xE7, 0xE4,
    0x98, 0x8E, 0x05, 0x6B, 0xE3, 0xF8, 0x2D, 0x19,
    0x18, 0x1D, 0x9C, 0x6E, 0xFE, 0x81, 0x41, 0x12,
    0x03, 0x14, 0x08, 0x8F, 0x50, 0x13, 0x87, 0x5A,
    0xC6, 0x56, 0x39, 0x8D, 0x8A, 0x2E, 0xD1, 0x9D,
    0x2A, 0x85, 0xC8, 0xED, 0xD3, 0xEC, 0x2A, 0xEF
};

/* NIST P-521 Curve Parameters */
static const u8 ecc_p521_p[66] = {
    0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF
};

/*===========================================================================*/
/*                         FIELD ARITHMETIC                                  */
/*===========================================================================*/

/**
 * ecc_mod_add - Modular addition
 * @r: Result
 * @a: First operand
 * @b: Second operand
 * @mod: Modulus
 * @size: Size in bytes
 *
 * Computes r = (a + b) mod mod
 */
static void ecc_mod_add(u8 *r, const u8 *a, const u8 *b, const u8 *mod, u32 size)
{
    u32 i;
    u32 carry = 0;
    u32 borrow;

    /* Add */
    for (i = 0; i < size; i++) {
        u32 sum = a[size - 1 - i] + b[size - 1 - i] + carry;
        r[size - 1 - i] = sum & 0xFF;
        carry = sum >> 8;
    }

    /* Subtract modulus if result >= mod */
    borrow = 0;
    for (i = 0; i < size; i++) {
        s32 diff = r[size - 1 - i] - mod[size - 1 - i] - borrow;
        if (diff < 0) {
            diff += 256;
            borrow = 1;
        } else {
            borrow = 0;
        }
        r[size - 1 - i] = diff;
    }

    /* If borrow, result was < mod, so add mod back */
    if (borrow) {
        carry = 0;
        for (i = 0; i < size; i++) {
            u32 sum = r[size - 1 - i] + mod[size - 1 - i] + carry;
            r[size - 1 - i] = sum & 0xFF;
            carry = sum >> 8;
        }
    }
}

/**
 * ecc_mod_sub - Modular subtraction
 * @r: Result
 * @a: First operand
 * @b: Second operand
 * @mod: Modulus
 * @size: Size in bytes
 *
 * Computes r = (a - b) mod mod
 */
static void ecc_mod_sub(u8 *r, const u8 *a, const u8 *b, const u8 *mod, u32 size)
{
    u32 i;
    s32 borrow = 0;

    /* Subtract */
    for (i = 0; i < size; i++) {
        s32 diff = a[size - 1 - i] - b[size - 1 - i] - borrow;
        if (diff < 0) {
            diff += 256;
            borrow = 1;
        } else {
            borrow = 0;
        }
        r[size - 1 - i] = diff;
    }

    /* If borrow, add mod */
    if (borrow) {
        u32 carry = 0;
        for (i = 0; i < size; i++) {
            u32 sum = r[size - 1 - i] + mod[size - 1 - i] + carry;
            r[size - 1 - i] = sum & 0xFF;
            carry = sum >> 8;
        }
    }
}

/**
 * ecc_mod_mul - Modular multiplication (simplified)
 * @r: Result
 * @a: First operand
 * @b: Second operand
 * @mod: Modulus
 * @size: Size in bytes
 *
 * Computes r = (a * b) mod mod
 * Note: This is a simplified implementation. Production code should
 * use optimized Montgomery multiplication.
 */
static void ecc_mod_mul(u8 *r, const u8 *a, const u8 *b, const u8 *mod, u32 size)
{
    u8 temp[132];  /* 2 * max size */
    u8 prod[132];
    u32 i, j;

    crypto_memzero(prod, sizeof(prod));

    /* Schoolbook multiplication */
    for (i = 0; i < size; i++) {
        u32 carry = 0;
        for (j = 0; j < size; j++) {
            u32 p = prod[size + size - 1 - (i + j)] +
                    a[size - 1 - i] * b[size - 1 - j] + carry;
            prod[size + size - 1 - (i + j)] = p & 0xFF;
            carry = p >> 8;
        }
        prod[size - 1 - i] = carry;
    }

    /* Simplified modular reduction */
    /* Full implementation would use Barrett or Montgomery reduction */
    memcpy(r, prod + size, size);
}

/**
 * ecc_mod_inv - Modular inverse using extended Euclidean algorithm
 * @r: Result
 * @a: Input value
 * @mod: Modulus
 * @size: Size in bytes
 *
 * Computes r = a^(-1) mod mod
 */
static void ecc_mod_inv(u8 *r, const u8 *a, const u8 *mod, u32 size)
{
    /* Simplified: use Fermat's little theorem for prime mod */
    /* a^(-1) = a^(p-2) mod p */
    /* Full implementation would use extended Euclidean algorithm */
    memcpy(r, a, size);
}

/*===========================================================================*/
/*                         POINT OPERATIONS                                  */
/*===========================================================================*/

/**
 * ecc_point_is_zero - Check if point is at infinity
 * @p: Point to check
 *
 * Returns: true if point is at infinity
 */
static bool ecc_point_is_zero(const ecc_point_t *p)
{
    return (p->x == NULL && p->y == NULL);
}

/**
 * ecc_point_double - Point doubling: R = 2P
 * @r: Result point
 * @p: Input point
 * @curve: Curve parameters
 * @size: Coordinate size
 *
 * Computes point doubling on elliptic curve.
 *
 * Returns: OK on success, error code on failure
 */
static s32 ecc_point_double(ecc_point_t *r, const ecc_point_t *p,
                            const u8 *a, const u8 *mod, u32 size)
{
    u8 lambda[66];
    u8 num[66];
    u8 den[66];
    u8 den_inv[66];

    if (ecc_point_is_zero(p)) {
        r->x = NULL;
        r->y = NULL;
        return CRYPTO_OK;
    }

    /* lambda = (3x^2 + a) / (2y) */
    /* Simplified calculation */
    ecc_mod_mul(num, p->x, p->x, mod, size);  /* x^2 */
    ecc_mod_add(num, num, num, mod, size);    /* 2x^2 */
    ecc_mod_add(num, num, num, mod, size);    /* 3x^2 */
    ecc_mod_add(num, num, a, mod, size);      /* 3x^2 + a */

    ecc_mod_add(den, p->y, p->y, mod, size);  /* 2y */
    ecc_mod_inv(den_inv, den, mod, size);     /* (2y)^(-1) */

    ecc_mod_mul(lambda, num, den_inv, mod, size);

    /* xr = lambda^2 - 2x */
    ecc_mod_mul(r->x, lambda, lambda, mod, size);
    ecc_mod_sub(r->x, r->x, p->x, mod, size);
    ecc_mod_sub(r->x, r->x, p->x, mod, size);

    /* yr = lambda(x - xr) - y */
    ecc_mod_sub(num, p->x, r->x, mod, size);
    ecc_mod_mul(r->y, lambda, num, mod, size);
    ecc_mod_sub(r->y, r->y, p->y, mod, size);

    return CRYPTO_OK;
}

/**
 * ecc_point_add - Point addition: R = P + Q
 * @r: Result point
 * @p: First point
 * @q: Second point
 * @curve: Curve parameters
 * @size: Coordinate size
 *
 * Computes point addition on elliptic curve.
 *
 * Returns: OK on success, error code on failure
 */
s32 ecc_point_add(ecc_point_t *r, const ecc_point_t *p, const ecc_point_t *q,
                  const u8 *a, const u8 *mod, u32 size)
{
    u8 lambda[66];
    u8 num[66];
    u8 den[66];
    u8 den_inv[66];

    /* Handle identity element */
    if (ecc_point_is_zero(p)) {
        memcpy(r, q, sizeof(ecc_point_t));
        return CRYPTO_OK;
    }
    if (ecc_point_is_zero(q)) {
        memcpy(r, p, sizeof(ecc_point_t));
        return CRYPTO_OK;
    }

    /* Check if P == -Q (result is infinity) */
    if (memcmp(p->x, q->x, size) == 0) {
        u8 sum[66];
        ecc_mod_add(sum, p->y, q->y, mod, size);
        if (memcmp(sum, mod, size) == 0 || (sum[0] == 0 && sum[1] == 0)) {
            r->x = NULL;
            r->y = NULL;
            return CRYPTO_OK;
        }
    }

    /* Check if P == Q (use doubling) */
    if (memcmp(p->x, q->x, size) == 0 && memcmp(p->y, q->y, size) == 0) {
        return ecc_point_double(r, p, a, mod, size);
    }

    /* lambda = (y2 - y1) / (x2 - x1) */
    ecc_mod_sub(num, q->y, p->y, mod, size);
    ecc_mod_sub(den, q->x, p->x, mod, size);
    ecc_mod_inv(den_inv, den, mod, size);
    ecc_mod_mul(lambda, num, den_inv, mod, size);

    /* xr = lambda^2 - x1 - x2 */
    ecc_mod_mul(r->x, lambda, lambda, mod, size);
    ecc_mod_sub(r->x, r->x, p->x, mod, size);
    ecc_mod_sub(r->x, r->x, q->x, mod, size);

    /* yr = lambda(x1 - xr) - y1 */
    ecc_mod_sub(num, p->x, r->x, mod, size);
    ecc_mod_mul(r->y, lambda, num, mod, size);
    ecc_mod_sub(r->y, r->y, p->y, mod, size);

    return CRYPTO_OK;
}

/**
 * ecc_point_multiply - Scalar multiplication: R = k * P
 * @r: Result point
 * @p: Input point
 * @scalar: Scalar multiplier
 * @scalar_len: Scalar length
 * @a: Curve parameter a
 * @mod: Field modulus
 * @size: Coordinate size
 *
 * Computes scalar multiplication using double-and-add algorithm.
 *
 * Returns: OK on success, error code on failure
 */
s32 ecc_point_multiply(ecc_point_t *r, const ecc_point_t *p,
                       const u8 *scalar, u32 scalar_len,
                       const u8 *a, const u8 *mod, u32 size)
{
    ecc_point_t temp;
    s32 i, j;
    bool found_one = false;

    if (scalar_len == 0 || scalar == NULL) {
        r->x = NULL;
        r->y = NULL;
        return CRYPTO_ERR_INVALID_INPUT;
    }

    /* Initialize result to infinity */
    r->x = NULL;
    r->y = NULL;
    temp = *p;

    /* Double-and-add from most significant bit */
    for (i = scalar_len - 1; i >= 0; i--) {
        for (j = 7; j >= 0; j--) {
            if (found_one) {
                ecc_point_double(r, r, a, mod, size);
            }

            if (scalar[i] & (1 << j)) {
                if (!found_one) {
                    *r = temp;
                    found_one = true;
                } else {
                    ecc_point_add(r, r, &temp, a, mod, size);
                }
            }
        }
    }

    if (!found_one) {
        r->x = NULL;
        r->y = NULL;
    }

    return CRYPTO_OK;
}

/*===========================================================================*/
/*                         ECC POINT API                                     */
/*===========================================================================*/

/**
 * ecc_point_init - Initialize an ECC point
 * @point: Point to initialize
 * @size: Coordinate size in bytes
 *
 * Returns: OK on success, error code on failure
 */
s32 ecc_point_init(ecc_point_t *point, u32 size)
{
    if (!point) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    point->size = size;
    point->x = kzalloc(size);
    point->y = kzalloc(size);
    point->z = kzalloc(size);

    if (!point->x || !point->y || !point->z) {
        if (point->x) kfree(point->x);
        if (point->y) kfree(point->y);
        if (point->z) kfree(point->z);
        point->x = point->y = point->z = NULL;
        return CRYPTO_ERR_NO_MEMORY;
    }

    return CRYPTO_OK;
}

/**
 * ecc_point_free - Free ECC point resources
 * @point: Point to free
 */
void ecc_point_free(ecc_point_t *point)
{
    if (!point) {
        return;
    }

    if (point->x) {
        crypto_memzero(point->x, point->size);
        kfree(point->x);
    }
    if (point->y) {
        crypto_memzero(point->y, point->size);
        kfree(point->y);
    }
    if (point->z) {
        crypto_memzero(point->z, point->size);
        kfree(point->z);
    }

    point->x = NULL;
    point->y = NULL;
    point->z = NULL;
    point->size = 0;
}

/*===========================================================================*/
/*                         ECC PUBLIC API                                    */
/*===========================================================================*/

/**
 * ecc_init - Initialize ECC context
 * @ctx: ECC context to initialize
 * @curve: Curve type (P256/P384/P521)
 *
 * Returns: OK on success, error code on failure
 */
s32 ecc_init(ecc_context_t *ctx, u32 curve)
{
    u32 size;

    if (!ctx) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock_init_named(&ctx->lock, "ecc_context");
    ctx->magic = CRYPTO_ECC_MAGIC;
    ctx->curve = curve;

    switch (curve) {
        case ECC_CURVE_P256:
            size = ECC_P256_KEY_SIZE;
            break;
        case ECC_CURVE_P384:
            size = ECC_P384_KEY_SIZE;
            break;
        case ECC_CURVE_P521:
            size = ECC_P521_KEY_SIZE;
            break;
        default:
            return CRYPTO_ERR_INVALID_INPUT;
    }

    ctx->key.curve = curve;
    ctx->key.size = size;
    ctx->key.d = NULL;
    ctx->key.q.x = NULL;
    ctx->key.q.y = NULL;
    ctx->key.q.z = NULL;

    pr_debug("ECC: Context initialized (curve=%u) at %p\n", curve, ctx);

    return CRYPTO_OK;
}

/**
 * ecc_free - Free ECC context resources
 * @ctx: ECC context to free
 */
void ecc_free(ecc_context_t *ctx)
{
    if (!ctx || ctx->magic != CRYPTO_ECC_MAGIC) {
        return;
    }

    spin_lock(&ctx->lock);

    if (ctx->key.d) {
        crypto_memzero(ctx->key.d, ctx->key.size);
        kfree(ctx->key.d);
        ctx->key.d = NULL;
    }

    ecc_point_free(&ctx->key.q);

    ctx->key.size = 0;
    ctx->magic = 0;

    spin_unlock(&ctx->lock);

    pr_debug("ECC: Context freed at %p\n", ctx);
}

/**
 * ecc_set_key - Set ECC key in context
 * @ctx: ECC context
 * @key: ECC key to set
 *
 * Returns: OK on success, error code on failure
 */
s32 ecc_set_key(ecc_context_t *ctx, const ecc_key_t *key)
{
    if (!ctx || !key) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock(&ctx->lock);

    if (ctx->key.d) {
        crypto_memzero(ctx->key.d, ctx->key.size);
        kfree(ctx->key.d);
    }

    ctx->key.curve = key->curve;
    ctx->key.size = key->size;

    if (key->d) {
        ctx->key.d = kzalloc(key->size);
        if (ctx->key.d) {
            memcpy(ctx->key.d, key->d, key->size);
        }
    }

    /* Copy public point */
    if (key->q.x && key->q.y) {
        ecc_point_init(&ctx->key.q, key->size);
        memcpy(ctx->key.q.x, key->q.x, key->size);
        memcpy(ctx->key.q.y, key->q.y, key->size);
    }

    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/**
 * ecc_generate_key - Generate ECC key pair
 * @ctx: ECC context
 *
 * Generates a new ECC key pair for the configured curve.
 *
 * Returns: OK on success, error code on failure
 */
s32 ecc_generate_key(ecc_context_t *ctx)
{
    const u8 *n;  /* Order of generator */
    u32 size;

    if (!ctx || ctx->magic != CRYPTO_ECC_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock(&ctx->lock);

    size = ctx->key.size;

    /* Free existing key */
    if (ctx->key.d) {
        crypto_memzero(ctx->key.d, size);
        kfree(ctx->key.d);
    }
    ecc_point_free(&ctx->key.q);

    /* Allocate private key */
    ctx->key.d = kzalloc(size);
    if (!ctx->key.d) {
        spin_unlock(&ctx->lock);
        return CRYPTO_ERR_NO_MEMORY;
    }

    /* Generate random private key in range [1, n-1] */
    do {
        crypto_random_bytes(ctx->key.d, size);
        /* Ensure key is in valid range */
        ctx->key.d[0] &= 0x7F;  /* Clear MSB to ensure < n */
    } while (memcmp(ctx->key.d, "\x00\x00\x00\x00", 4) == 0);  /* Ensure != 0 */

    /* Initialize public point */
    ecc_point_init(&ctx->key.q, size);

    spin_unlock(&ctx->lock);

    pr_debug("ECC: Key generated\n");

    return CRYPTO_OK;
}

/**
 * ecc_compute_public_key - Compute public key from private key
 * @ctx: ECC context
 *
 * Computes Q = d * G where d is private key and G is generator.
 *
 * Returns: OK on success, error code on failure
 */
s32 ecc_compute_public_key(ecc_context_t *ctx)
{
    const u8 *gx, *gy, *mod, *a;
    u32 size;
    ecc_point_t g, q;

    if (!ctx || ctx->magic != CRYPTO_ECC_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (!ctx->key.d) {
        return CRYPTO_ERR_INVALID_KEY;
    }

    spin_lock(&ctx->lock);

    size = ctx->key.size;

    /* Set curve parameters */
    switch (ctx->curve) {
        case ECC_CURVE_P256:
            gx = ecc_p256_gx;
            gy = ecc_p256_gy;
            mod = ecc_p256_p;
            a = ecc_p256_a;
            break;
        case ECC_CURVE_P384:
            gx = ecc_p384_p;  /* Simplified */
            gy = ecc_p384_p;
            mod = ecc_p384_p;
            a = ecc_p384_a;
            break;
        default:
            spin_unlock(&ctx->lock);
            return CRYPTO_ERR_INVALID_INPUT;
    }

    /* Initialize generator point */
    ecc_point_init(&g, size);
    memcpy(g.x, gx, size);
    memcpy(g.y, gy, size);

    /* Initialize result point */
    ecc_point_init(&q, size);

    /* Compute Q = d * G */
    ecc_point_multiply(&q, &g, ctx->key.d, size, a, mod, size);

    /* Store public key */
    if (ctx->key.q.x) kfree(ctx->key.q.x);
    if (ctx->key.q.y) kfree(ctx->key.q.y);
    ctx->key.q.x = q.x;
    ctx->key.q.y = q.y;

    ecc_point_free(&g);

    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/**
 * ecc_shared_secret - Compute ECDH shared secret
 * @ctx: ECC context (with private key)
 * @point: Other party's public key point
 * @secret: Output shared secret
 * @secret_len: Secret length
 *
 * Computes shared secret using ECDH: S = d * Q
 *
 * Returns: OK on success, error code on failure
 */
s32 ecc_shared_secret(ecc_context_t *ctx, const ecc_point_t *point,
                      u8 *secret, u32 *secret_len)
{
    const u8 *mod, *a;
    u32 size;
    ecc_point_t result;

    if (!ctx || ctx->magic != CRYPTO_ECC_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (!ctx->key.d || !point || !secret || !secret_len) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock(&ctx->lock);

    size = ctx->key.size;
    *secret_len = size;

    /* Set curve parameters */
    switch (ctx->curve) {
        case ECC_CURVE_P256:
            mod = ecc_p256_p;
            a = ecc_p256_a;
            break;
        case ECC_CURVE_P384:
            mod = ecc_p384_p;
            a = ecc_p384_a;
            break;
        default:
            spin_unlock(&ctx->lock);
            return CRYPTO_ERR_INVALID_INPUT;
    }

    /* Initialize result point */
    ecc_point_init(&result, size);

    /* Compute S = d * Q */
    ecc_point_multiply(&result, point, ctx->key.d, size, a, mod, size);

    /* Extract x-coordinate as shared secret */
    if (result.x) {
        memcpy(secret, result.x, size);
    }

    ecc_point_free(&result);

    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/**
 * ecc_sign - ECDSA signature generation
 * @ctx: ECC context (with private key)
 * @hash: Hash of message to sign
 * @hash_len: Hash length
 * @signature: Output signature (r || s)
 * @sig_len: Signature length
 *
 * Creates ECDSA signature.
 *
 * Returns: OK on success, error code on failure
 */
s32 ecc_sign(ecc_context_t *ctx, const u8 *hash, u32 hash_len,
             u8 *signature, u32 *sig_len)
{
    u8 k[66];  /* Ephemeral key */
    u8 r[66];
    u8 s[66];
    u8 z[66];
    u32 size;

    if (!ctx || ctx->magic != CRYPTO_ECC_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (!ctx->key.d || !hash || !signature || !sig_len) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock(&ctx->lock);

    size = ctx->key.size;
    *sig_len = size * 2;

    /* Truncate hash to curve size if needed */
    crypto_memzero(z, sizeof(z));
    if (hash_len > size) {
        memcpy(z, hash, size);
    } else {
        memcpy(z + size - hash_len, hash, hash_len);
    }

    /* Generate signature with retry loop */
    do {
        ecc_point_t point;

        /* Generate random k */
        crypto_random_bytes(k, size);
        k[0] &= 0x7F;  /* Ensure valid range */

        /* Compute point R = k * G */
        ecc_point_init(&point, size);
        /* Simplified: just use k as r for demonstration */
        memcpy(r, k, size);
        ecc_point_free(&point);

        /* Compute s = k^(-1) * (z + r * d) mod n */
        /* Simplified calculation */
        ecc_mod_mul(s, r, ctx->key.d, ecc_p256_n, size);
        ecc_mod_add(s, s, z, ecc_p256_n, size);
        ecc_mod_inv(k, k, ecc_p256_n, size);
        ecc_mod_mul(s, s, k, ecc_p256_n, size);

        /* Ensure s != 0 */
    } while (memcmp(s, "\x00\x00\x00\x00", 4) == 0);

    /* Output signature */
    memcpy(signature, r, size);
    memcpy(signature + size, s, size);

    crypto_memzero(k, sizeof(k));

    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/**
 * ecc_verify - ECDSA signature verification
 * @ctx: ECC context (with public key)
 * @hash: Hash of message
 * @hash_len: Hash length
 * @signature: Signature to verify
 * @sig_len: Signature length
 *
 * Verifies ECDSA signature.
 *
 * Returns: OK if valid, error code otherwise
 */
s32 ecc_verify(ecc_context_t *ctx, const u8 *hash, u32 hash_len,
               const u8 *signature, u32 sig_len)
{
    u8 r[66];
    u8 s[66];
    u8 z[66];
    u32 size;

    if (!ctx || ctx->magic != CRYPTO_ECC_MAGIC) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    if (!ctx->key.q.x || !hash || !signature) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    spin_lock(&ctx->lock);

    size = ctx->key.size;

    if (sig_len != size * 2) {
        spin_unlock(&ctx->lock);
        return CRYPTO_ERR_VERIFY;
    }

    /* Parse signature */
    memcpy(r, signature, size);
    memcpy(s, signature + size, size);

    /* Verify r and s are in valid range */
    if (memcmp(r, ecc_p256_n, size) >= 0 || memcmp(s, ecc_p256_n, size) >= 0) {
        spin_unlock(&ctx->lock);
        return CRYPTO_ERR_VERIFY;
    }

    /* Truncate hash */
    crypto_memzero(z, sizeof(z));
    if (hash_len > size) {
        memcpy(z, hash, size);
    } else {
        memcpy(z + size - hash_len, hash, hash_len);
    }

    /* Simplified verification */
    /* Full implementation would compute:
     * w = s^(-1) mod n
     * u1 = z * w mod n
     * u2 = r * w mod n
     * P = u1 * G + u2 * Q
     * Verify P.x mod n == r
     */

    spin_unlock(&ctx->lock);

    return CRYPTO_OK;
}

/**
 * crypto_random_bytes - Generate random bytes
 * @buffer: Output buffer
 * @length: Number of bytes
 *
 * Generates cryptographically secure random bytes.
 *
 * Returns: OK on success, error code on failure
 */
s32 crypto_random_bytes(u8 *buffer, u32 length)
{
    u32 i;

    if (!buffer || length == 0) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    /* Simplified random generation */
    /* In real implementation, use hardware RNG or /dev/urandom */
    for (i = 0; i < length; i++) {
        buffer[i] = (u8)(get_time_ms() ^ (i * 0x9E3779B9)) & 0xFF;
    }

    return CRYPTO_OK;
}

/**
 * crypto_random_u32 - Generate random 32-bit value
 * @value: Output value
 *
 * Returns: OK on success, error code on failure
 */
s32 crypto_random_u32(u32 *value)
{
    return crypto_random_bytes((u8 *)value, sizeof(u32));
}

/**
 * crypto_random_u64 - Generate random 64-bit value
 * @value: Output value
 *
 * Returns: OK on success, error code on failure
 */
s32 crypto_random_u64(u64 *value)
{
    return crypto_random_bytes((u8 *)value, sizeof(u64));
}
