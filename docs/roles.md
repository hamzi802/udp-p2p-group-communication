# Your part of the project

All four members write code for the **same app**. These roles are work assignments, not separate computers or servers. Start with your own standalone demo. Integration comes later.

## M1 — Musab: UDP messages

Work in `libs/rudp/` and `apps/rudp_demo/`. Make two local processes send messages. Add message numbers, ACKs, retries and duplicate protection. Show what happens when one message is lost. Your task is [#49](https://github.com/hamzi802/udp-p2p-group-communication/issues/49).

## M2 — Hamza: peers and chat

Work in `libs/membership/`, `libs/chat/` and `apps/peer/`. First store four pretend peers and print them with `/peers`. Then send one chat message to three pretend peers through a fake sender. No UDP is needed yet. Your task is [#48](https://github.com/hamzi802/udp-p2p-group-communication/issues/48).

## M3 — Ali: file storage

Work in `libs/hashing/`, `libs/filestore/` and `apps/chunk_tool/`. Split a local file into chunks, hash each chunk, save and rebuild it. Show which two pretend peers should hold each chunk. No network is needed yet. Your task is [#51](https://github.com/hamzi802/udp-p2p-group-communication/issues/51).

## M4 — Hamza Yousuf: downloads

Work in `libs/filetransfer/` and `apps/download_demo/`. Ask two fake sources for different chunks and rebuild a file. Stop one source halfway; finish from the other source. No real UDP is needed yet. Your task is [#50](https://github.com/hamzi802/udp-p2p-group-communication/issues/50).

## After all four demos

Work together on [#52 integration](https://github.com/hamzi802/udp-p2p-group-communication/issues/52) and [#53 final demo](https://github.com/hamzi802/udp-p2p-group-communication/issues/53). Each member connects and explains their own module. See the [simple plan](project-plan.md) for dates and the full demonstration checklist.
