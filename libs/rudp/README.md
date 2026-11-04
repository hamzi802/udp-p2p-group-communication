# rudp

Owner: M1 (@Musab-Farooq).

This is a buildable scaffold, not a completed implementation. Public headers go in `include/rudp/`, implementation in `src/`, and behavioral tests in `tests/`.

Tasks:
- I08: Packet encoding, decoding and validation
- I09: UDP socket wrapper and two-process demo
- I16: ACK tracking, retries, backoff and failure reporting
- I17: Sequence ordering and duplicate suppression
- I18: Separate streams, bounded queues and receive window
- I31: Restart, stale-packet and queue-failure handling

Use interfaces from common to avoid circular library dependencies. See [architecture](../../docs/architecture.md) and [team workflow](../../CONTRIBUTING.md).
