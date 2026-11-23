# Setup status

The repository has a C++17/CMake skeleton, issue and PR templates, CODEOWNERS and CI. M3's local hashing, chunk storage, placement and message-service implementation is present and has registered behavioral tests. A WSL Debug/Ninja build and all five registered CTest cases passed. M1 reliable transport, M2 membership/chat/PeerApp and M4 transfer implementations remain scaffolds, so the end-to-end peer application and full group demo are not complete.

The active plan has [six issues](project-plan.md): one standalone module demo for each member, then integration and a final four-computer demonstration. The older 44 detailed proposal issues are closed and kept for reference. Do not work on those separately.

The three active milestones use proposed internal dates of 15, 22 and 29 November 2026 (Asia/Karachi); they are internal targets, not confirmed instructor deadlines.

Repository rules such as mandatory human approval and required CI checks may still need an owner to configure them. CODEOWNERS alone does not enforce approval. A teammate should review each PR before merge.
