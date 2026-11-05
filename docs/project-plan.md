# Simple team plan

Everyone builds their own part first. Use fake peers, fake messages or local files so nobody waits for another member. The four parts meet only in the integration task.

| Step | Target date (Asia/Karachi) | What is finished |
|---|---|---|
| 1. Build separately | 15 Nov 2026 | Each member can run and explain a standalone demo of their own module. |
| 2. Connect parts | 22 Nov 2026 | One peer app uses all four modules together. |
| 3. Demonstrate | 29 Nov 2026 | Four computers, eight files, peer departure, fifth peer joining and failed-source download are tested and reported. |

These are proposed internal dates, not confirmed instructor deadlines.

## Step 1: four independent tasks

| Member | Issue | Your output | Test without teammates' code |
|---|---|---|---|
| M1 @Musab-Farooq | [#49 UDP message demo](https://github.com/hamzi802/udp-p2p-group-communication/issues/49) | Two programs send messages with ACK/retry and no duplicate display. | Run two local processes and simulate loss. |
| M2 @hamzi802 | [#48 Peer list and chat](https://github.com/hamzi802/udp-p2p-group-communication/issues/48) | Add/remove peers, /peers and chat fan-out. | Use four pretend peers and a fake message sender. |
| M3 @Byte-sized-Ali | [#51 File chunks and storage](https://github.com/hamzi802/udp-p2p-group-communication/issues/51) | Split, hash, store and rebuild a file; choose two holders. | Use one local test file and pretend peers. |
| M4 @hamzayousuf717-ops | [#50 Two-source download](https://github.com/hamzi802/udp-p2p-group-communication/issues/50) | Fetch chunks from two sources and recover if one stops. | Use two fake sources in memory. |

Each issue says exactly what to start with and what counts as done. Put your work in your own library and demo app, run your tests, and submit a small PR. Do not wait for someone else's finished module. Share the functions and message fields your integration partner will need.

## Step 2: integration

[#52 Connect the four modules](https://github.com/hamzi802/udp-p2p-group-communication/issues/52) starts **after** the four demos. M2 coordinates, all four members help connect their own part, and M1 reviews. Connect chat to UDP first, then file storage and transfer. This is the only stage where one member's code needs another's.

## Step 3: final proof

[#53 Four-computer demo and report](https://github.com/hamzi802/udp-p2p-group-communication/issues/53) is coordinated by M4 with help from all members. Record the commands and actual results. Everyone must be able to explain their part and how the parts connect.

The previous 44 issues are closed as archived technical notes. They are not the active task list.
