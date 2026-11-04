# Protocol draft — M1 owns I03

**Status: proposed layout, not frozen and not implemented.** Agree all decisions below with M2–M4 before writing codecs.

| Offset | Bytes | Field |
|---|---|---|
| 0 | 2 | magic |
| 2 | 1 | version |
| 3 | 1 | type |
| 4 | 1 | flags |
| 5 | 1 | stream |
| 6 | 2 | receive window |
| 8 | 4 | sender ID |
| 12 | 8 | sender session |
| 20 | 8 | receiver session |
| 28 | 4 | sequence |
| 32 | 4 | ACK: next expected sequence |
| 36 | 4 | SACK bitmap |
| 40 | 2 | payload length |
| 42 | 2 | reserved |
| 44 | 4 | CRC32 |

Total header: 48 bytes. Proposed maximum application datagram: 1200 bytes including this header (1152 bytes remain for all payload metadata and bytes). Use network byte order and explicit encoding, never object memory.

Stream 0 carries control/chat; stream 1 carries file data. ACK packets do not consume reliable sequence numbers or require ACKs. SACK bit i describes ack+1+i. Reject out-of-session or invalid-length messages before dispatch.

## Message families

- M2: JOIN, WELCOME, HELLO, HEARTBEAT, LEAVE, PEER_LIST_REQUEST, PEER_LIST_PAGE, CHAT.
- M1: ACK.
- M3: CATALOG_REQUEST, FILE_ANNOUNCE, MANIFEST_PAGE, HAVE_PAGE.
- M4: CHUNK_REQUEST, CHUNK_DATA, CHUNK_ERROR.

## Decisions to freeze

- Magic value, numeric types, flags, CRC variant/known test vector (CRC field zero during calculation).
- Exact payload layouts, length/page limits and metadata consistency.
- Nonces and session handshake including WELCOME nonce echo and receiver-session zero discovery.
- Per-session/stream sequence initialization, wraparound comparisons and restart handling.
- Retry/backoff limits, ACK/SACK validation, queue bounds, receive-window semantics and fairness.
- Error results, callbacks, cancellation and missing ordered-message policy.

CRC and hashes are not authentication/encryption. Unknown senders, malformed metadata and stale messages must be handled explicitly.
