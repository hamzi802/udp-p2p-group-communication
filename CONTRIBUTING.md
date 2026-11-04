# Team workflow

1. Pick one assigned issue and read its acceptance checklist and dependencies.
2. Create a focused branch from current main, e.g. `feature/11-bootstrap-join` using the actual issue number.
3. Implement your module yourself. Use test fakes when dependencies are unfinished.
4. Configure, build and run CTest. Add meaningful behavioral tests for the feature.
5. Open a PR with behavior, test evidence and `Closes #<actual issue number>`.
6. M1 requests M2; M2 requests M1; M3 and M4 request both. Request Copilot as extra review if eligible.
7. Address review comments. Merge only after an independent human approval and passing checks. Authors cannot approve their own PRs.
8. Update your issue with progress/blockers and explain your module to a teammate.

CODEOWNERS requests reviews but does not enforce approval. I06 tracks enabling repository rules (one human approval, code-owner review, resolved conversations and passing CI) and eligible Copilot settings. Do not count Copilot approval as the required human approval.

Shared interfaces belong in common. Discuss interface/protocol changes with affected owners before implementation. Do not create circular links between concrete libraries.

Use descriptive commits and your own GitHub identity. Never commit build output, chunk stores, downloaded/shared user files, secrets or private machine configuration.
