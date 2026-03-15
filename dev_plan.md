# SoleSlicer NP: Development Plan for 5.3-Codex

## Goal

Build `SoleSlicer NP`: an `OrcaSlicer`-based fork with ZAA support (Z anti-aliasing / Z contouring), focused on Linux CLI usage rather than desktop UI.

The required end state is:

- a public GitHub repository under `nick-yudin/SoleSlicer-NP`;
- source work performed through that repository;
- Linux CLI slicing support with ZAA;
- GitHub Actions builds, not local Mac builds;
- a Docker image that runs on a local Intel MacBook Pro 2017;
- a reproducible CLI slicing workflow that produces G-code with working ZAA behavior.

## Repository Requirement

All development must happen through the public repository:

- GitHub owner: `nick-yudin`
- Repository slug: `SoleSlicer-NP`
- Repository URL target: `https://github.com/nick-yudin/SoleSlicer-NP`

Note:

- GitHub repository slugs cannot reliably use spaces, so `SoleSlicer NP` should be represented as `SoleSlicer-NP` in the actual repository URL and git remote configuration.

## Verified Facts

- Session date: `2026-03-15`.
- Latest stable `OrcaSlicer` release at the time of review: `v2.3.1`.
- Latest `BambuStudio-ZAA` release at the time of review: `zaa-v1.0.3`, published on `2025-06-27`.
- `BambuStudio-ZAA` is a fork of `bambulab/BambuStudio`, not a fork of `OrcaSlicer`.
- The `zaa` branch in `BambuStudio-ZAA` is `16` commits ahead of its BambuStudio base around version `02.01.01.52` (base commit `456a87d`, dated `2025-06-16`).
- Latest upstream `BambuStudio` release at the time of review: `v02.05.00.67`.
- The ZAA fork changes `124` files in total, but the important functional logic is concentrated in `libslic3r`, config definitions, and slicing pipeline integration.
- The key algorithm file added by ZAA is `src/libslic3r/ContourZ.cpp`.
- Key ZAA config options identified in the fork:
  - `zaa_enabled`
  - `zaa_minimize_perimeter_height`
  - `zaa_dont_alternate_fill_direction`
  - `zaa_min_z`
  - `zaa_region_disable`
- `OrcaSlicer` already contains a CLI code path in `src/OrcaSlicer.cpp`.
- `OrcaSlicer` has a CMake option `SLIC3R_GUI`, but a fully headless build is not guaranteed to work without additional fixes.

## Chosen Strategy

Do not attempt a direct merge of `BambuStudio-ZAA` into current `OrcaSlicer`.

Use a selective port strategy instead:

1. Start from `OrcaSlicer v2.3.1`.
2. Port only the ZAA logic, config entries, and minimal slicing pipeline changes required for CLI operation.
3. Do not spend the first iteration on full desktop UI support.
4. Treat Linux CLI inside Docker as the primary deliverable, even if the build stage still requires some GUI-related dependencies.
5. Only pursue a truly headless `SLIC3R_GUI=OFF` build if it is cheap after the MVP is working.

## Why This Strategy Is Preferred

- `BambuStudio-ZAA` is based on an older `BambuStudio` branch, not on modern `OrcaSlicer`.
- A direct merge would introduce a large amount of unrelated conflict resolution work.
- The ZAA behavior appears localized enough to port manually or reimplement where necessary.
- The user requirement is server-side slicing through CLI and Docker, not a desktop application.

## What Must Be Ported from ZAA

### Required

- `src/libslic3r/ContourZ.cpp`
- ZAA config definitions in `PrintConfig`
- ZAA integration into the slicing pipeline at object/layer/region level
- support for storing contoured paths such as `z_contoured`
- G-code generation support for variable-Z extrusion paths
- `zaa_dont_alternate_fill_direction`
- `zaa_minimize_perimeter_height`
- any necessary top-surface, ironing, and perimeter handling that is part of actual ZAA behavior

### Not Required in the First Pass

- full UI for ZAA settings
- demo resources, screenshots, STL/SCAD examples, homepage assets
- desktop installers or AppImage packaging
- unrelated cosmetic or branding changes from the ZAA fork

## Execution Plan for 5.3-Codex

### Phase 0. Repository Setup

- Create the public repository `nick-yudin/SoleSlicer-NP`.
- Initialize local git state if needed.
- Add `origin` pointing to `https://github.com/nick-yudin/SoleSlicer-NP.git`.
- Use this repository for all subsequent work.

Success criteria:

- the public repository exists;
- the local workspace is connected to it through `origin`.

### Phase 1. Prepare the Base

- Create a new `SoleSlicer NP` codebase from `OrcaSlicer v2.3.1`.
- Configure remotes for reference work:
  - `orca` -> `OrcaSlicer/OrcaSlicer`
  - `zaa` -> `adob/BambuStudio-ZAA`
  - `bambu` -> `bambulab/BambuStudio`
- Produce a source-focused diff between:
  - `bambu/master@02.01.01.52`
  - `zaa/zaa`
- Separate changes into:
  - core code changes;
  - UI-only changes;
  - test/demo resources;
  - CI/release changes.

Deliverable:

- `docs/zaa-port-notes.md` with file and symbol mapping for the port.

### Phase 2. Validate the CLI-Only Route

- Identify the exact current CLI flow in `OrcaSlicer`:
  - entrypoints;
  - required flags;
  - supported input formats;
  - minimum viable slicing scenario.
- Test two build modes:
  - option A: `SLIC3R_GUI=OFF`
  - option B: standard Linux build, CLI-only usage in runtime container
- If option A fails quickly, do not block the project on it.
- Record the MVP decision explicitly:
  - `Phase 1 target = CLI runtime image`
  - `Phase 2 optional = true headless build`

Deliverable:

- documented CLI smoke path;
- chosen build mode for the MVP.

### Phase 3. Port the ZAA Config Layer

- Port the config definitions into:
  - `src/libslic3r/PrintConfig.hpp`
  - `src/libslic3r/PrintConfig.cpp`
  - `src/libslic3r/Preset.cpp`
- Keep UI wiring optional.
- If registration or config loading requires additional symbols, solve that at config/preset level without depending on a full desktop UI.
- Add sane defaults and valid ranges.

Validation:

- CLI can load a profile containing ZAA parameters without crashing.
- config serialization and deserialization still work.

### Phase 4. Port the ZAA Core Algorithm

- Port `src/libslic3r/ContourZ.cpp`.
- Port the required dependent changes in:
  - `ExtrusionEntity`
  - `Layer`
  - `PrintObjectSlice`
  - `Print`
  - `GCode`
  - `Fill`
  - geometry helpers such as `Clipper`-related code where needed
- Do not port by filename blindly.
- Port by behavior, symbols, and required type changes.
- Minimize the change set so it fits current `OrcaSlicer`.

Validation:

- the project builds;
- the slicing pipeline runs on at least one simple STL;
- disabling ZAA does not regress normal slicing behavior.

### Phase 5. Integrate ZAA into G-code Output

- Verify that generated G-code actually varies `Z` within a nominal layer where expected.
- Check:
  - top solid infill;
  - ironing;
  - external perimeter;
  - perimeter height minimization behavior.
- Confirm that `zaa_min_z` and region disable logic are applied correctly.
- If `zaa_dont_alternate_fill_direction` is necessary for output quality, make sure it is part of the active path rather than a dead config option.

Validation:

- at least one test model produces variable-Z moves in the top layer G-code;
- with `zaa_enabled=false`, output remains equivalent in intent to standard slicing.

### Phase 6. Build the Linux CLI Artifact

- Skip desktop packaging.
- Produce a Linux target suitable for container execution.
- If needed, use a multi-stage Docker build:
  - builder image with the full toolchain;
  - runtime image with the binary, required resources, and minimal runtime libraries.
- First platform target: `linux/amd64`.

Deliverable:

- Docker image `soleslicer-np` with a working CLI entrypoint.

### Phase 7. Set Up GitHub Actions

- Add a workflow that on push or tag:
  - builds the Linux artifact;
  - builds the Docker image;
  - publishes the image to `ghcr.io`;
  - stores artifacts and logs.
- Do not spend time on Windows or macOS packaging in the first pass.
- Add dependency caching to reduce build cost.

Desired result:

- tags like `v0.1.0`
- image tags such as `ghcr.io/<owner>/soleslicer-np:<tag>`
- image tag `ghcr.io/<owner>/soleslicer-np:latest`

### Phase 8. Validate on the Local Mac via Docker

- Write the minimum usage instructions:
  - mount model directory;
  - mount profiles or presets;
  - run CLI slicing command;
  - retrieve `gcode`.
- Validate locally on the Intel MacBook 2017 using Docker.
- Keep one or two small regression models for smoke testing.

## Definition of Done

The work is complete only if all of the following are true:

1. A public repository `nick-yudin/SoleSlicer-NP` exists and is used as the working repository.
2. The implementation is based on modern `OrcaSlicer`, not on an old `BambuStudio` branch.
3. A working Linux CLI path exists.
4. A Docker image runs locally on the Intel MacBook 2017 through Docker.
5. At least one reproducible CLI slicing command is documented.
6. ZAA measurably changes the top-layer G-code with variable-Z behavior.
7. Standard slicing without ZAA is not broken.
8. The build is performed through GitHub Actions, not on the local Mac.

## Risks and Decision Rules

### Risk 1. `SLIC3R_GUI=OFF` does not become viable quickly

Decision:

- do not block the MVP;
- ship a Dockerized CLI runtime first;
- treat true headless compilation as an optional follow-up.

### Risk 2. ZAA depends on older internal `BambuStudio` structures

Decision:

- do not transplant diffs blindly;
- port behavior and the minimum required structural changes;
- rewrite parts against current `OrcaSlicer` architecture where necessary.

### Risk 3. Too many GUI references exist around config keys

Decision:

- prioritize CLI config loading and slicing behavior;
- leave UI wiring stubbed or omitted if it is not required for the CLI build.

### Risk 4. It is hard to prove that ZAA really works

Decision:

- keep one or two dedicated test models with sloped top surfaces;
- compare standard G-code against ZAA G-code;
- verify variable `Z` movement inside top-layer segments directly.

## Minimum Technical Deliverables

- `docs/zaa-port-notes.md`
- `Dockerfile`
- `.github/workflows/build-linux-cli.yml`
- a short CLI usage section in `README.md`
- small regression test assets
- a published `ghcr.io` image

## What Not To Do in the First Pass

- do not port the full ZAA desktop UI one-to-one;
- do not build a polished desktop installer;
- do not target all operating systems at once;
- do not carry large demo assets unless required for validation;
- do not perform a direct merge of the old ZAA fork into current `OrcaSlicer`.

## Recommended Prompt for 5.3-Codex

```text
Task: build SoleSlicer NP, an OrcaSlicer-based fork with ZAA support (Z contouring / variable-height top surface), using ideas and code from https://github.com/adob/BambuStudio-ZAA.

Repository requirements:
- Work through the public repository https://github.com/nick-yudin/SoleSlicer-NP
- If the repository does not exist yet, create it first and wire the local workspace to origin.

Constraints and goals:
- Base: current stable OrcaSlicer v2.3.1.
- Do not direct-merge the old BambuStudio-ZAA fork into modern OrcaSlicer.
- Use a selective port strategy for the required ZAA behavior.
- UI is not the target. Linux CLI is the target.
- Builds must run through GitHub Actions.
- The final result must run locally on an Intel MacBook Pro 2017 through Docker.
- The final result must be a dockerized CLI slicer that can slice a model with ZAA and emit G-code.

What is already known:
- BambuStudio-ZAA is a fork of bambulab/BambuStudio, not OrcaSlicer.
- The ZAA branch is 16 commits ahead of a BambuStudio 02.01.01.52 base.
- The key algorithm file is src/libslic3r/ContourZ.cpp.
- Key config options: zaa_enabled, zaa_minimize_perimeter_height, zaa_dont_alternate_fill_direction, zaa_min_z, zaa_region_disable.
- OrcaSlicer already has a CLI code path in src/OrcaSlicer.cpp.
- SLIC3R_GUI exists, but do not block on a true-headless build if a CLI runtime container is faster to ship.

Execution plan:
1. Create or connect to the public repository nick-yudin/SoleSlicer-NP.
2. Freeze the base at OrcaSlicer v2.3.1.
3. Produce a source-only ZAA diff against the corresponding BambuStudio base.
4. Write docs/zaa-port-notes.md with symbol-level dependencies.
5. Port the ZAA config layer.
6. Port the core algorithm and required pipeline/G-code changes.
7. Make the CLI slicing path work.
8. Build a linux/amd64 Docker image.
9. Add GitHub Actions workflows for artifact and GHCR publishing.
10. Validate local Docker execution and document the command.

Success criteria:
- Linux CLI works.
- Docker image works locally on the Mac.
- ZAA really changes the top-layer G-code.
- Normal slicing without ZAA is not broken.

Decision rule:
- Prefer selective rewrite/port over merge-conflict archaeology.
- If true-headless build is expensive, ship the CLI container first and clean up later.
```
