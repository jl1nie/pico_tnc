# Repository instructions

This repository contains firmware for the `pico_tnc` project.
Keep changes small, reviewable, and easy to revert.

## Priorities
1. Preserve existing firmware behavior unless the task explicitly changes it.
2. Prefer minimal diffs over large refactors.
3. Keep memory usage and queue sizes visible when changing text output paths.
4. When changing user-visible commands, update both `README.md` and `README_JP.md` if needed.

## Working rules
- Prefer one feature per commit.
- Do not rename unrelated files.
- Avoid broad formatting-only edits.
- Keep comments concise and technical.
- When adding generated data, keep the generator script in the repo.

## Build and validation
- Build the firmware after any non-trivial change.
- If a build cannot be run, record that fact in `WORKLOG.md`.
- When changing USB/TTY/output code, note possible RAM impact and queue behavior in `WORKLOG.md`.

## Logging after each task
After making code changes, update `WORKLOG.md` with:
- date
- summary of requested change
- files changed
- behavior changes
- validation status
- remaining risks or TODOs

If the task is part of a multi-step effort, also update `PLAN.md`.

## Generated files
- Put generator scripts under `tools/`.
- Mark generated files clearly at the top of the file.
- If a generated table is updated, update the generator script in the same change.

## Current project-specific plan
The current planned work around help output is tracked in `PLAN.md`.
