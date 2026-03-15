# SoleSlicer NP Linux CLI + Docker

This document describes the Linux CLI-first workflow for `SoleSlicer NP`.

## Image

The CI publishes a runtime image to GHCR:

- `ghcr.io/nick-yudin/soleslicer-np-cli:latest` (main branch)
- `ghcr.io/nick-yudin/soleslicer-np-cli:<git-sha>`

## Pull

```bash
docker pull ghcr.io/nick-yudin/soleslicer-np-cli:latest
```

## Slice Through Docker

Run from your model directory:

```bash
docker run --rm \
  -v "$PWD:/work" \
  ghcr.io/nick-yudin/soleslicer-np-cli:latest \
  --slice 1 \
  --outputdir /work/out \
  --zaa_enabled 1 \
  --zaa_min_z 0.03 \
  /work/model.stl
```

The resulting file is typically:

- `/work/out/plate_1.gcode`

## Baseline vs ZAA Quick Check

Baseline:

```bash
docker run --rm \
  -v "$PWD:/work" \
  ghcr.io/nick-yudin/soleslicer-np-cli:latest \
  --slice 1 \
  --outputdir /work/out-baseline \
  /work/model.stl
```

ZAA enabled:

```bash
docker run --rm \
  -v "$PWD:/work" \
  ghcr.io/nick-yudin/soleslicer-np-cli:latest \
  --slice 1 \
  --outputdir /work/out-zaa \
  --zaa_enabled 1 \
  --zaa_min_z 0.03 \
  /work/model.stl
```

Compare:

```bash
diff -u out-baseline/plate_1.gcode out-zaa/plate_1.gcode | head -n 200
```

In a successful ZAA path, the ZAA output contains more extrusion commands with explicit `Z` on top-surface-related segments.
