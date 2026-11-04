# Setup status

The initial foundation commit supplies the C++17/CMake module layout, role documentation, issue/PR templates, M1/M2 CODEOWNERS and CI. These are scaffolding, not completed protocol/application features.

The 44 proposal tasks are real GitHub issues assigned to the four confirmed accounts. The `Sync project milestones` workflow creates the five proposed milestones, attaches issues, adds role labels, and resolves dependency links from actual issue numbers. Check its run result before treating milestones as configured.

## Still requires team action

- I02/I03/I04: agree and implement exact interfaces, protocol and metadata formats.
- I05: create/configure the optional GitHub Project board and its status/target-date views.
- I06: enable main-branch protection/rules (PR, one independent human/code-owner approval, resolved conversations, passing `build-and-test`), then eligible Copilot review settings. CODEOWNERS alone does not enforce these.
- All feature issues: code, tests and actual acceptance evidence remain the assigned member's work.
- Confirm internal checkpoint dates against the instructor's actual submission deadline.

## Planning automation

`.github/project-plan.json` maps proposal IDs to real GitHub issue numbers. The initial setup branch and subsequent updates on main trigger a narrowly scoped workflow using GitHub's temporary token (contents:read, issues:write). It can also be rerun through Actions → Sync project milestones → Run workflow.

It does not create duplicate issues, close tasks, change repository access or enforce branch rules. It preserves existing issue checkboxes, comments, extra labels and additional assignees. Update the plan deliberately if target dates or owners change.
