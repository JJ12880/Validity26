#ifndef BITCOIN_CHAINPARAMSSEEDS_H
#define BITCOIN_CHAINPARAMSSEEDS_H
/**
 * List of fixed seed nodes for the Validity network
 *
 * Each line contains a BIP155 serialized (networkID, addr, port) tuple.
 * Port 27913 = 0x6d09 (big-endian: 0x6d,0x09)
 *
 * VALIDITY-PORT: Seeded from known Validity node 141.98.255.139:27913
 */
static const uint8_t chainparams_seed_main[] = {
    // 141.98.255.139:27913 (0x8d62ff8b, port 0x6d09)
    0x01,0x04,0x8d,0x62,0xff,0x8b,0x6d,0x09,
};

static const uint8_t chainparams_seed_test[] = {
};
#endif // BITCOIN_CHAINPARAMSSEEDS_H
