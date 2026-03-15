# ZAA Port Notes

## Objective

Port the ZAA behavior from `adob/BambuStudio-ZAA` into modern `OrcaSlicer` with the minimum change set required for Linux CLI slicing.

## Reference Repositories

- target base: `OrcaSlicer v2.3.1`
- feature source: `adob/BambuStudio-ZAA` branch `zaa`
- compatibility reference: `bambulab/BambuStudio`

## Known ZAA Config Keys

- `zaa_enabled`
- `zaa_minimize_perimeter_height`
- `zaa_dont_alternate_fill_direction`
- `zaa_min_z`
- `zaa_region_disable`

## Known ZAA Commit Sequence

The ZAA branch is short enough to inspect commit-by-commit. Important commits already identified:

- `9597e06` Initial implementation
- `5d71c05` perimeter height reduction and fixes
- `5ac1ba5` ironing support
- `ccb6868` `zaa_region_disable` and fixes
- `0ca27fc` restore `0.03` error allowance

## High-Value Files From the ZAA Fork

These are the first files to inspect and port:

- `src/libslic3r/ContourZ.cpp`
- `src/libslic3r/PrintConfig.hpp`
- `src/libslic3r/PrintConfig.cpp`
- `src/libslic3r/Preset.cpp`
- `src/libslic3r/ExtrusionEntity.hpp`
- `src/libslic3r/ExtrusionEntity.cpp`
- `src/libslic3r/Layer.hpp`
- `src/libslic3r/PrintObjectSlice.cpp`
- `src/libslic3r/PrintObject.cpp`
- `src/libslic3r/Print.cpp`
- `src/libslic3r/GCode.cpp`
- `src/libslic3r/Fill/Fill.cpp`

## Behavior Notes From the Existing ZAA Implementation

- the core algorithm samples along extrusion segments and raycasts against the mesh;
- it modifies top-surface and selected perimeter paths by applying per-point Z offsets;
- it marks paths as `z_contoured`;
- it supports top solid infill, ironing, and selected perimeter handling;
- it includes extra logic for minimizing external perimeter height on steep slopes;
- it limits Z displacement using `zaa_min_z` and a tolerance band.

## Current Porting Rules

- do not port the entire fork blindly;
- do not prioritize UI changes;
- port behavior first, UI later;
- keep normal slicing behavior unchanged when `zaa_enabled` is off;
- validate using generated G-code, not only by code review.

## Expected Deliverables From the Next Porting Pass

1. a symbol-level mapping between ZAA-only changes and current `OrcaSlicer` structures
2. a first compile-ready config-layer port
3. a decision on whether CLI can ship with `SLIC3R_GUI=OFF` or requires a standard build with CLI-only runtime usage
