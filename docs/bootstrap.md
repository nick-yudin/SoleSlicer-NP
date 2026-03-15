# SoleSlicer NP Bootstrap Notes

## Repository

- public repository: `https://github.com/nick-yudin/SoleSlicer-NP`
- default working branch: `main`
- initial bootstrap strategy: merge `OrcaSlicer v2.3.1` into the public repository without rewriting the already-published history

## Git Remotes

- `origin` -> `https://github.com/nick-yudin/SoleSlicer-NP.git`
- `orca` -> `https://github.com/OrcaSlicer/OrcaSlicer.git`
- `zaa` -> `https://github.com/adob/BambuStudio-ZAA.git`
- `bambu` -> `https://github.com/bambulab/BambuStudio.git`

## Bootstrap Baseline

- `OrcaSlicer` tag used for bootstrap: `v2.3.1`
- `BambuStudio-ZAA` reference branch: `zaa`
- `BambuStudio` reference branch: `master`

## Current Scope

The repository is now in a valid state for ZAA port work:

- the public repository exists;
- the local workspace is connected to `origin`;
- `OrcaSlicer v2.3.1` has been merged into `main`;
- the project plan is tracked in `dev_plan.md`.

## Next Technical Step

Use this repository to:

1. map the ZAA diff against the older BambuStudio base;
2. identify the smallest code path required for Linux CLI slicing;
3. port the ZAA config layer and core algorithm into current `OrcaSlicer`;
4. validate the result through Docker.
