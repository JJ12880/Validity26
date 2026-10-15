#ifndef BITCOIN_CHAINPARAMSSEEDS_H
#define BITCOIN_CHAINPARAMSSEEDS_H
/**
 * List of fixed seed nodes for the Validity network
 *
 * Each line contains a BIP155 serialized (networkID, addr, port) tuple.
 * Port 27913 = 0x6d09 (big-endian: 0x6d,0x09)
 *
 * VALIDITY-PORT: the 0.13 client's pnSeed6_main list (the project's seed nodes),
 * generated from contrib/seeds/nodes_main.txt.
 */
static const uint8_t chainparams_seed_main[] = {
    0x01,0x04,0xd1,0xfa,0xf3,0xad,0x6d,0x09, // 209.250.243.173:27913
    0x01,0x04,0x2d,0x20,0xd7,0xca,0x6d,0x09, // 45.32.215.202:27913
    0x01,0x04,0xd1,0xfa,0xea,0x49,0x6d,0x09, // 209.250.234.73:27913
    0x01,0x04,0x2d,0x4d,0xeb,0x14,0x6d,0x09, // 45.77.235.20:27913
    0x01,0x04,0x6c,0x3d,0xbb,0xb4,0x6d,0x09, // 108.61.187.180:27913
};

static const uint8_t chainparams_seed_test[] = {
};
#endif // BITCOIN_CHAINPARAMSSEEDS_H
