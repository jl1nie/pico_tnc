#include "mona_backend_minimal.h"

#include <stdint.h>
#include <string.h>

#include "libmona_pico/mona_compat.h"

typedef struct {
    uint32_t state[8];
    uint64_t bit_len;
    uint8_t data[64];
    uint32_t data_len;
} sha256_ctx_t;

static const uint32_t k256[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

static inline uint32_t rotr32(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static inline uint32_t bsig0(uint32_t x) { return rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22); }
static inline uint32_t bsig1(uint32_t x) { return rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25); }
static inline uint32_t ssig0(uint32_t x) { return rotr32(x, 7) ^ rotr32(x, 18) ^ (x >> 3); }
static inline uint32_t ssig1(uint32_t x) { return rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10); }

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t data[64])
{
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;

    for (int i = 0; i < 16; ++i) {
        w[i] = ((uint32_t)data[i * 4] << 24) |
               ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) |
               (uint32_t)data[i * 4 + 3];
    }
    for (int i = 16; i < 64; ++i) {
        w[i] = ssig1(w[i - 2]) + w[i - 7] + ssig0(w[i - 15]) + w[i - 16];
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t t1 = h + bsig1(e) + ch(e, f, g) + k256[i] + w[i];
        uint32_t t2 = bsig0(a) + maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(sha256_ctx_t *ctx)
{
    ctx->data_len = 0;
    ctx->bit_len = 0;
    ctx->state[0] = 0x6a09e667u;
    ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u;
    ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu;
    ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu;
    ctx->state[7] = 0x5be0cd19u;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        ctx->data[ctx->data_len++] = data[i];
        if (ctx->data_len == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bit_len += 512;
            ctx->data_len = 0;
        }
    }
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t out[32])
{
    uint32_t i = ctx->data_len;

    if (ctx->data_len < 56) {
        ctx->data[i++] = 0x80u;
        while (i < 56) ctx->data[i++] = 0x00u;
    } else {
        ctx->data[i++] = 0x80u;
        while (i < 64) ctx->data[i++] = 0x00u;
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }

    ctx->bit_len += (uint64_t)ctx->data_len * 8u;
    ctx->data[63] = (uint8_t)(ctx->bit_len);
    ctx->data[62] = (uint8_t)(ctx->bit_len >> 8);
    ctx->data[61] = (uint8_t)(ctx->bit_len >> 16);
    ctx->data[60] = (uint8_t)(ctx->bit_len >> 24);
    ctx->data[59] = (uint8_t)(ctx->bit_len >> 32);
    ctx->data[58] = (uint8_t)(ctx->bit_len >> 40);
    ctx->data[57] = (uint8_t)(ctx->bit_len >> 48);
    ctx->data[56] = (uint8_t)(ctx->bit_len >> 56);
    sha256_transform(ctx, ctx->data);

    for (i = 0; i < 4; ++i) {
        out[i] = (uint8_t)((ctx->state[0] >> (24 - i * 8)) & 0xffu);
        out[i + 4] = (uint8_t)((ctx->state[1] >> (24 - i * 8)) & 0xffu);
        out[i + 8] = (uint8_t)((ctx->state[2] >> (24 - i * 8)) & 0xffu);
        out[i + 12] = (uint8_t)((ctx->state[3] >> (24 - i * 8)) & 0xffu);
        out[i + 16] = (uint8_t)((ctx->state[4] >> (24 - i * 8)) & 0xffu);
        out[i + 20] = (uint8_t)((ctx->state[5] >> (24 - i * 8)) & 0xffu);
        out[i + 24] = (uint8_t)((ctx->state[6] >> (24 - i * 8)) & 0xffu);
        out[i + 28] = (uint8_t)((ctx->state[7] >> (24 - i * 8)) & 0xffu);
    }
}

static int sha256_impl(const uint8_t *data, size_t len, uint8_t out32[32])
{
    sha256_ctx_t ctx;
    if (!data || !out32) return 0;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out32);
    return 1;
}

static int not_supported_160(const uint8_t *data, size_t len, uint8_t out20[20])
{
    (void)data;
    (void)len;
    (void)out20;
    return 0;
}

static int not_supported_hmac(const uint8_t *key, size_t key_len,
                              const uint8_t *data, size_t data_len,
                              uint8_t out32[32])
{
    (void)key;
    (void)key_len;
    (void)data;
    (void)data_len;
    (void)out32;
    return 0;
}

static int not_supported_rng(uint8_t *out, size_t len)
{
    (void)out;
    (void)len;
    return 0;
}

static int not_supported_pub(const uint8_t secret32[32], uint8_t out33[33])
{
    (void)secret32;
    (void)out33;
    return 0;
}

static int not_supported_sign(const uint8_t msg32[32],
                              const uint8_t secret32[32],
                              uint8_t out64[64],
                              int *out_recid)
{
    (void)msg32;
    (void)secret32;
    (void)out64;
    (void)out_recid;
    return 0;
}

static int not_supported_recover(const uint8_t msg32[32],
                                 const uint8_t sig64[64],
                                 int recid,
                                 uint8_t out33[33])
{
    (void)msg32;
    (void)sig64;
    (void)recid;
    (void)out33;
    return 0;
}

static int not_supported_verify(const uint8_t msg32[32],
                                const uint8_t sig64[64],
                                const uint8_t pub33[33])
{
    (void)msg32;
    (void)sig64;
    (void)pub33;
    return 0;
}

static const mona_crypto_vtable_t g_vt = {
    .sha256 = sha256_impl,
    .ripemd160 = not_supported_160,
    .hmac_sha256 = not_supported_hmac,
    .random_bytes = not_supported_rng,
    .secp_pubkey_create_compressed = not_supported_pub,
    .secp_sign_recoverable_compact = not_supported_sign,
    .secp_recover_pubkey_compressed = not_supported_recover,
    .secp_verify_compact = not_supported_verify,
};

void mona_backend_minimal_init(void)
{
    if (mona_get_crypto() != &g_vt) {
        mona_set_crypto(&g_vt);
    }
}

bool mona_backend_minimal_has_full_crypto(void)
{
    return false;
}
