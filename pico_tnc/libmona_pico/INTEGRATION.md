# Monacoin message-sign library for Pico-TNC

## Directory layout

- `include/mona_compat.h`
  - hardware-independent core API
- `src/mona_compat.c`
  - Base58/Bech32/WIF/address/sign/verify core logic
- `include/mona_pico_api.h`
  - Pico-TNC-friendly thin wrapper around `mona_compat`
- `src/mona_pico_api.c`
  - active address type / raw-or-WIF import helpers
- `tools/*`
  - PC validation CLI using OpenSSL backend

## What Pico-TNC should keep in NVM

Store only the key slot; do not store derived strings.

```c
typedef struct {
    uint8_t secret[32];
    bool compressed;
    mona_addr_type_t active_type;
    bool valid;
} mona_keyslot_t;
```

## Suggested command mapping

- `privkey gen [m|p|mona1|p2pkh|p2sh|p2wpkh]`
- `privkey set <WIF or RAW>`
- `privkey type [m|p|mona1|p2pkh|p2sh|p2wpkh]`
- `privkey show`
- `signmessage <text>`
- `verifymessage <address> <sig> <text>`

## Electrum Mona comparison summary

Relevant files:
- `electrum_mona/ecc.py`
- `electrum_mona/bitcoin.py`
- `electrum_mona/tests/test_bitcoin.py`

Observed behaviour that matches this library:

1. Message magic is `"\x19Monacoin Signed Message:\n" + varint(len) + message`.
2. WIF import/export accepts typed strings such as `p2wpkh:...` and `p2wpkh-p2sh:...`.
3. Signing currently uses legacy compact headers `27..34` even for segwit addresses.
4. Verification accepts BIP137-style `35..42` headers too, and if no explicit type is encoded in the header,
   it checks all of `p2pkh`, `p2wpkh`, and `p2wpkh-p2sh` derived addresses.
5. Electrum Mona has tests showing the same base64 signature for the same key/message across p2wpkh and p2wpkh-p2sh,
   because the signed payload is only the message hash and the header does not encode the type when Electrum itself signs.

## Important note

This library intentionally mirrors the PHP implementation and Electrum Mona compatibility mode:
- sign: emits legacy header style (`27 + recid + (compressed ? 4 : 0)`)
- verify: accepts both legacy and BIP137-style segwit headers
