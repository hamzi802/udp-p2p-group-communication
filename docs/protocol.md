# Protocol specification v1 (M1 / I03)

**Status: PROPOSED v1.0. Not frozen until M2, M3 and M4 have signed off (section 13).**
Owner: M1 (header, sessions, reliable delivery, `ACK`). Payload layouts of every other message are owned by the member named in section 6.

The key words MUST, MUST NOT, SHOULD and MAY are used in the usual RFC 2119 sense. Anything a receiver finds invalid is **dropped silently** (never answered) and counted in a per-reason statistics counter.

---

## 1. Conventions

- **Byte order:** network byte order (big-endian) for every multi-byte field, including 64-bit session IDs.
- **Encoding:** explicit field-by-field serialization into a byte buffer. Never cast a struct to bytes, never use `#pragma pack`, never read a field without a bounds check.
- **Integers:** `u8`, `u16`, `u32`, `u64` are unsigned. Sequence numbers use serial-number arithmetic (section 9.1).
- **One message = one datagram.** There is no fragmentation or reassembly in v1. Applications split large data themselves (e.g. chunks into blocks).
- **Peer:** one running process, identified by `sender_id`.
- **Link:** the relationship between two peers. A link has two endpoints, each with its own 64-bit session. A **link incarnation** is one successful handshake (section 8); all state (sequence numbers, windows, timers) belongs to one incarnation.
- **Channel:** reliable state for one `(link incarnation, stream)`. Each link has exactly two channels (stream 0 and stream 1), each directional half independent.

## 2. Constants

| Name | Value | Meaning |
|---|---|---|
| `MAGIC` | `0x5032` ("P2") | first two bytes of every datagram |
| `VERSION` | `1` | protocol version |
| `HEADER_SIZE` | 48 | bytes |
| `MAX_DATAGRAM` | 1200 | bytes, header included |
| `MAX_PAYLOAD` | 1152 | `MAX_DATAGRAM - HEADER_SIZE` |
| `RECV_BUFFER` | 2048 | size of the buffer given to `recvfrom`, so oversized datagrams are detected, not truncated |
| `WINDOW_MAX` | 32 | packets per channel, in flight (sender) and buffered (receiver) |
| `SEND_QUEUE_MAX` | 256 | packets queued but not yet transmitted, per channel |
| `RTO_INIT` | 500 ms | used until the first RTT sample |
| `RTO_MIN` / `RTO_MAX` | 200 ms / 4000 ms | clamp for the retransmission timeout |
| `MAX_RETRIES` | 8 | retransmissions of one packet before the link fails |
| `FAST_RETX_DUP` | 3 | later packets SACKed before a fast retransmit |
| `PENDING_TTL` | 5 s | lifetime of a half-open handshake entry |
| `PENDING_MAX` | 32 | half-open handshake entries per peer |
| `PENDING_PER_SRC` | 4 | half-open entries per source IP |
| `JOIN_MIN_PAYLOAD` | 64 | bytes, JOIN is padded to this |
| `WELCOME_MAX_DATAGRAM` | size of the triggering JOIN | anti-amplification rule |

Heartbeat interval, failure thresholds and JOIN retry schedule belong to M2.

## 3. Header (48 bytes)

| Offset | Bytes | Field | Rules |
|---|---|---|---|
| 0 | 2 | `magic` | MUST be `0x5032` |
| 2 | 1 | `version` | MUST be `1`; other values are dropped (no negotiation in v1) |
| 3 | 1 | `type` | must be in the registry (section 6) |
| 4 | 1 | `flags` | section 4 |
| 5 | 1 | `stream` | `0` or `1` only |
| 6 | 2 | `window` | free reorder slots (0..32) the sender of this packet offers on `stream`; only meaningful with `ACK_VALID`, otherwise MUST be 0 |
| 8 | 4 | `sender_id` | MUST NOT be 0 |
| 12 | 8 | `sender_session` | MUST NOT be 0 |
| 20 | 8 | `receiver_session` | 0 only in `JOIN`; in all other types MUST equal the receiver's session for this link |
| 28 | 4 | `seq` | reliable packets: >= 1; all others: 0 |
| 32 | 4 | `ack` | with `ACK_VALID`: next expected reliable `seq` on `stream` (everything below it has been received); otherwise 0 |
| 36 | 4 | `sack` | with `ACK_VALID`: bit *i* set means `ack + 1 + i` was received; otherwise 0. Bit 31 MUST be 0 |
| 40 | 2 | `payload_len` | MUST equal `datagram_length - 48` and be <= 1152 |
| 42 | 2 | `reserved` | MUST be 0 |
| 44 | 4 | `crc32` | section 3.1 |

### 3.1 CRC32

CRC-32/IEEE 802.3 (the zlib / Ethernet CRC): polynomial `0x04C11DB7`, reflected input and output, initial value `0xFFFFFFFF`, final XOR `0xFFFFFFFF`.
It is computed over the **whole datagram** (48 header bytes followed by the payload) **with the `crc32` field set to zero**, then stored big-endian.
Check value: CRC32 of the ASCII bytes `123456789` is **`0xCBF43926`**.

The CRC detects accidental corruption only. It is not authentication (section 11).

## 4. Flags (`flags` byte)

| Bit | Name | Meaning |
|---|---|---|
| 0 | `RELIABLE` | packet consumes a sequence number and MUST be acknowledged |
| 1 | `ACK_VALID` | `ack`, `sack` and `window` carry data (piggybacked or in an `ACK` packet) |
| 2 | `PROBE` | only on `ACK` packets: "reply with an ACK now" (used for zero-window probing) |
| 3-7 | - | MUST be 0; packets with these bits set are dropped |

`ACK` packets MUST have `ACK_VALID` set and `RELIABLE` clear. `JOIN` and `WELCOME` MUST have `ACK_VALID` clear (no link exists yet). Any other packet MAY set `ACK_VALID` to piggyback acknowledgements for the **reverse direction of the same stream**.

## 5. Streams

| Stream | Purpose | Sequence space |
|---|---|---|
| 0 | control and chat | own, per link incarnation and direction |
| 1 | bulk file data | own, per link incarnation and direction |

Streams are independent: delivery order is guaranteed **within** a stream only, and a stalled stream 1 never blocks stream 0. When both streams have packets ready, the sender alternates one packet from each (round robin), so neither starves. Congestion control is out of scope; flow control is the `window` field.

## 6. Message type registry

Ranges: `0x00-0x0F` M1, `0x10-0x2F` M2, `0x30-0x4F` M3, `0x50-0x6F` M4, `0x70-0xFF` reserved (dropped as unknown). Type `0x00` is invalid.

| Type | Name | Owner | Reliable | Stream | Notes |
|---|---|---|---|---|---|
| `0x01` | ACK | M1 | no | 0 or 1 (the acknowledged stream) | no payload |
| `0x10` | JOIN | M2 | no | 0 | `receiver_session = 0`; payload >= 64 bytes |
| `0x11` | WELCOME | M2 | no | 0 | payload starts with 8-byte cookie (section 8) |
| `0x12` | HELLO | M2 | yes | 0 | payload starts with 8-byte cookie echo |
| `0x13` | HEARTBEAT | M2 | no | 0 | unicast on an established link |
| `0x14` | LEAVE | M2 | yes | 0 | |
| `0x15` | PEER_LIST_REQUEST | M2 | yes | 0 | |
| `0x16` | PEER_LIST_PAGE | M2 | yes | 0 | |
| `0x20` | CHAT | M2 | yes | 0 | |
| `0x30` | CATALOG_REQUEST | M3 | yes | 0 | |
| `0x31` | FILE_ANNOUNCE | M3 | yes | 0 | |
| `0x32` | MANIFEST_PAGE | M3 | yes | 1 | |
| `0x33` | HAVE_PAGE | M3 | yes | 1 | |
| `0x50` | CHUNK_REQUEST | M4 | yes | 1 | |
| `0x51` | CHUNK_DATA | M4 | yes | 1 | |
| `0x52` | CHUNK_ERROR | M4 | yes | 1 | |

A packet whose `RELIABLE` flag or `stream` disagrees with its row is dropped.

## 7. Receive validation order

Every received datagram goes through these steps in order, in a single function. The first failing step drops the packet and increments its counter. Nothing is allocated before step 11.

**Stateless (`parse_header`)**

| # | Check | Counter |
|---|---|---|
| 1 | `48 <= datagram_length <= 1200` | `E_LENGTH_RANGE` |
| 2 | `magic == 0x5032` | `E_MAGIC` |
| 3 | `version == 1` | `E_VERSION` |
| 4 | `reserved == 0` and `flags & 0xF8 == 0` | `E_RESERVED` |
| 5 | `stream` in {0, 1} | `E_STREAM` |
| 6 | `type` in registry, and `RELIABLE` / `stream` match its row | `E_TYPE` |
| 7 | `payload_len == datagram_length - 48` | `E_PAYLOAD_LEN` |
| 8 | field consistency: `sender_id != 0`, `sender_session != 0`; `RELIABLE` iff `seq != 0`; without `ACK_VALID`: `ack == sack == window == 0`; `window <= 32`; `sack` bit 31 clear; `PROBE` only on `ACK`; `ACK` has `ACK_VALID`; `JOIN` and `WELCOME` have `ACK_VALID` clear; `receiver_session == 0` only for `JOIN` | `E_FIELDS` |
| 9 | CRC32 | `E_CRC` |

**Stateful (`link_accept`)**

| # | Check | Counter |
|---|---|---|
| 10 | unknown `sender_id` is accepted only for `JOIN`, `WELCOME` and `HELLO` (which go through the handshake, section 8) | `E_UNKNOWN_PEER` |
| 11 | source IP:port equals the address recorded for this link | `E_ADDRESS` |
| 12 | `receiver_session` equals my session for this link, and `sender_session` equals the recorded peer session | `E_SESSION` |
| 13 | with `ACK_VALID`: `send_una <= ack <= send_next` (serial arithmetic), and every set `sack` bit refers to a sequence number that was already sent | `E_ACK` |
| 14 | reliable data: `seq` in `[recv_next, recv_next + 31]` or `< recv_next` (duplicate, re-ACKed) | `E_SEQ_WINDOW` |

A packet that passes step 12 refreshes the peer's liveness timestamp (used by M2's failure detector).

## 8. Identity, sessions and handshake

**Identity.** At start-up each peer picks a random non-zero 32-bit `sender_id` from `getrandom()` (or `/dev/urandom`). It stays constant for the lifetime of the process. A restarted peer therefore appears as a new peer; the old one times out through heartbeats.

**Sessions.** For every link incarnation each endpoint picks a fresh random non-zero 64-bit session (same random source). Packets from X to Y carry X's session as `sender_session` and Y's session as `receiver_session`. A session is never reused, so packets from an earlier incarnation can never match a later one: stale and replayed traffic is rejected at step 12.

**Handshake** (J joins; A is any peer J reaches by multicast or `--bootstrap`; J repeats it with every peer it learns about, so no peer is special after joining):

1. **J -> A: `JOIN`**, unreliable, `sender_session = Sj`, `receiver_session = 0`, payload padded to >= 64 bytes. J retries on M2's schedule, always with the same `Sj`.
2. **A -> J: `WELCOME`**, unreliable, `sender_session = Sa` (fresh), `receiver_session = Sj`. The first 8 payload bytes are a random `cookie` that A stores in a pending entry keyed by `(source IP:port, Sj)`. The whole datagram MUST NOT be longer than the `JOIN` that triggered it. Repeated JOINs with the same key get the same `Sa` and cookie. J accepts a `WELCOME` only if `receiver_session == Sj` (this echo is the nonce check) and marks the link to A established.
3. **J -> A: `HELLO`**, reliable (`seq = 1`, stream 0), `sender_session = Sj`, `receiver_session = Sa`, first 8 payload bytes = the cookie. A accepts it only if the pending entry exists and the cookie matches; it then creates the link, ACKs the HELLO, and may from now on send reliable data (peer lists, catalog).

**Anti-amplification and resource limits.** Until step 3 completes, A sends only the small `WELCOME`. Pending entries expire after `PENDING_TTL`, are capped at `PENDING_MAX` in total and `PENDING_PER_SRC` per source IP; a JOIN beyond a cap is dropped.

**Restart and re-join.** A `JOIN` carrying a `sender_id` that has an established link is handled as a fresh handshake: it replaces the old link only if the new `HELLO` verifies **and** the source address is the same or the old link is not ALIVE. Otherwise it is dropped (collision guard). On replacement the upper layers get `link_failed(old)` then `link_established(new)`.

**Sequence initialisation.** The first reliable packet on each `(incarnation, stream, direction)` has `seq = 1`.

## 9. Reliable delivery

Guarantees per channel: **exactly-once, in-order** delivery of reliable messages. No ordering is promised across streams or across senders.

### 9.1 Sequence numbers
32-bit, compared with serial arithmetic: `a < b` iff `(int32_t)(a - b) < 0`. The window (32) is far below 2^31, so wraparound is safe. Unreliable packets use `seq = 0` and are never ACKed or retransmitted.

### 9.2 Sender
- In-flight limit = `min(WINDOW_MAX, peer_window)`. Further messages wait in a queue of at most `SEND_QUEUE_MAX`; beyond that `send()` returns `QUEUE_FULL`.
- Each transmitted packet is kept with `sent_at`, `retries`, `retransmitted` until acknowledged.
- A cumulative `ack` frees every packet below it; each set `sack` bit frees that packet.
- **RTT / RTO (RFC 6298):** first sample `SRTT = R`, `RTTVAR = R/2`; afterwards `RTTVAR = 3/4*RTTVAR + 1/4*|SRTT - R|`, `SRTT = 7/8*SRTT + 1/8*R`; `RTO = SRTT + 4*RTTVAR`, clamped to `[RTO_MIN, RTO_MAX]`. **Karn's rule:** no sample from a packet that was retransmitted. The estimator is per link, shared by both streams.
- **Timeout:** retransmit the packet, set `RTO = min(2*RTO, RTO_MAX)` (reset by the next valid sample), `retries++`.
- **Fast retransmit:** when `FAST_RETX_DUP` later packets are SACKed while an earlier one is unacknowledged, retransmit it once.
- **Failure:** `retries > MAX_RETRIES` on any packet means the link has **failed** (section 9.5).
- **Zero window:** when `peer_window == 0` the sender sends an `ACK` packet with `PROBE` every `RTO` until a non-zero window arrives. (ACKs are unreliable, so without probing a lost window update would deadlock the channel.)

### 9.3 Receiver
- On a valid reliable packet: if `seq < recv_next` or the slot is already filled, it is a duplicate, so discard the payload and re-ACK; if `seq` is within `[recv_next, recv_next+31]`, buffer it; otherwise drop (E_SEQ_WINDOW).
- While slot `recv_next` is filled: deliver to the application in order, `recv_next++`.
- **ACK policy (v1):** send an `ACK` immediately for every valid reliable packet (duplicates included), on the same stream: `ack = recv_next`, `sack` = bitmap of buffered packets, `window = 32 - buffered_out_of_order`. No delayed ACKs.
- Delivery is a synchronous callback; the application must not block in it (queue heavy work).
- On a `PROBE`, reply with an `ACK` immediately.

### 9.4 Cancellation
`cancel_queued(peer, stream)` drops only messages that have **never been transmitted**. Packets already in flight keep being retransmitted, because abandoning a sent sequence number would stall the receiver forever. Higher layers (M4) cancel a transfer by ignoring late data.

### 9.5 Link failure
When a link fails, M1 discards both channels and all queues for that incarnation, reports `link_failed(peer, reason)` once, and never delivers anything from that incarnation afterwards (no gaps are skipped, no partial delivery). Talking to that peer again requires a new handshake (new sessions).

## 10. Transport API contract (what M2-M4 call)

| Operation | Result codes |
|---|---|
| `send(peer, stream, type, payload)` | `OK`, `PEER_UNKNOWN`, `PAYLOAD_TOO_LARGE` (> 1152), `QUEUE_FULL`, `LINK_FAILED`, `BAD_TYPE` |
| `send_unreliable(peer, type, payload)` | as above, without `QUEUE_FULL` |
| `send_to_addr(addr, type, payload)` | multicast / bootstrap JOIN only |
| `cancel_queued(peer, stream)` | number of dropped messages |
| `on_message(peer, stream, type, payload)` | delivered in order, once |
| `on_link_established(peer)` / `on_link_failed(peer, reason)` | reasons: `RETRIES_EXHAUSTED`, `REPLACED`, `LEFT` |

## 11. Security notes and limits

Covered: accidental corruption (CRC), stale or restarted peers (sessions), blind off-path injection (a forged packet must guess a 64-bit session and a valid sequence number), amplification (cookie, size rule), memory exhaustion (fixed two streams, pending-entry caps, bounded queues).

Not covered in v1: an on-path or same-LAN attacker who can read traffic can learn sessions and forge packets; there is no authentication or encryption. HEARTBEAT replay inside a live session is M2's concern (use a monotonic counter in its payload and accept only newer values). A later version could add an HMAC trailer keyed by a group secret, or payload encryption (bonus feature).

## 12. Test vectors

All values hexadecimal. Generated by the script in the appendix. A correct codec MUST reproduce these bytes exactly.

**Vector A** - `CHAT`, `RELIABLE | ACK_VALID`, payload `"hi"` (50 bytes)
```
5032 01 20 03 00 0020 11223344 0102030405060708 a1a2a3a4a5a6a7a8
00000005 00000003 00000005 0002 0000 4d6d3d4e | 6869
```
Fields: magic, version 1, type 0x20, flags 0x03, stream 0, window 32, sender_id 0x11223344, sender_session, receiver_session, seq 5, ack 3, sack 0x5 (bits 0 and 2: seq 4 and 6 received), payload_len 2, reserved 0, crc 0x4d6d3d4e.

**Vector B** - pure `ACK` on stream 1, `ACK_VALID` (48 bytes)
```
5032 01 01 02 01 0011 11223344 0102030405060708 a1a2a3a4a5a6a7a8
00000000 00000009 00000003 0000 0000 c95e3262
```
seq 0, ack 9, sack 0x3 (seq 10 and 11 received), window 17, crc 0xc95e3262.

**Vector C** - `JOIN`, `receiver_session = 0`, payload = 64 zero bytes (header shown, 112 bytes in total, crc 0x86d5b20d)
```
5032 01 10 00 00 0000 deadbeef 1122334455667788 0000000000000000
00000000 00000000 00000000 0040 0000 86d5b20d
```

**CRC check:** `crc32("123456789") = 0xCBF43926`.

## 13. Decision register (the "Decisions to freeze" list)

| Decision | Settled in |
|---|---|
| Magic, numeric types, flags, CRC variant and test vector | sections 2, 3, 3.1, 4, 6, 12 |
| Exact payload layouts, length and page limits, metadata consistency | **open: M2, M3, M4** (each owner writes their sections; header-level limit is 1152 bytes) |
| Nonces and session handshake, WELCOME echo, receiver-session zero discovery | section 8 |
| Per-session / stream sequence initialisation, wraparound, restart handling | sections 8, 9.1 |
| Retry and backoff limits, ACK/SACK validation, queue bounds, receive-window semantics, fairness | sections 2, 5, 7, 9 |
| Error results, callbacks, cancellation, missing ordered-message policy | sections 9.4, 9.5, 10 |

**Sign-off needed from:**
- **M2:** JOIN/WELCOME/HELLO payload layouts (cookie prefix rule), HEARTBEAT as unicast per link, heartbeat counter, JOIN retry schedule, who re-handshakes after `link_failed`.
- **M3:** streams for `MANIFEST_PAGE` and `HAVE_PAGE` (proposed stream 1), page sizes.
- **M4:** use of `cancel_queued`, behaviour after `link_failed` mid-transfer.

## Appendix: reference generator (Python 3)

```python
import struct, zlib
FMT = '>HBBBBHIQQIIIHHI'          # 48 bytes, big-endian, no padding

def pack(type_, flags, stream, window, sender_id, ss, rs, seq, ack, sack, payload=b''):
    hdr = struct.pack(FMT, 0x5032, 1, type_, flags, stream, window,
                      sender_id, ss, rs, seq, ack, sack, len(payload), 0, 0)
    crc = zlib.crc32(hdr + payload) & 0xFFFFFFFF      # crc field is zero here
    return hdr[:44] + struct.pack('>I', crc) + payload

print(pack(0x20, 0x03, 0, 32, 0x11223344, 0x0102030405060708,
           0xA1A2A3A4A5A6A7A8, 5, 3, 0x5, b'hi').hex())      # Vector A
```
