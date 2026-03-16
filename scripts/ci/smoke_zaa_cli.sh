#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 /path/to/orca-slicer" >&2
    exit 2
fi

SLICER_BIN="$1"
if [[ ! -x "${SLICER_BIN}" ]]; then
    echo "Slicer binary is not executable: ${SLICER_BIN}" >&2
    exit 2
fi

WORK_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "${WORK_DIR}"
}
trap cleanup EXIT

MODEL_FILE="${WORK_DIR}/wedge.stl"
BASE_OUT="${WORK_DIR}/out-baseline"
ZAA_OUT="${WORK_DIR}/out-zaa"

mkdir -p "${BASE_OUT}" "${ZAA_OUT}"

cat > "${MODEL_FILE}" <<'EOF'
solid ramp
  facet normal 0 0 -1
    outer loop
      vertex 0 0 0
      vertex 120 0 0
      vertex 120 40 0
    endloop
  endfacet
  facet normal 0 0 -1
    outer loop
      vertex 0 0 0
      vertex 120 40 0
      vertex 0 40 0
    endloop
  endfacet
  facet normal 0 -1 0
    outer loop
      vertex 0 0 0
      vertex 120 0 0
      vertex 120 0 5
    endloop
  endfacet
  facet normal 0 -1 0
    outer loop
      vertex 0 0 0
      vertex 120 0 5
      vertex 0 0 0.6
    endloop
  endfacet
  facet normal 0 1 0
    outer loop
      vertex 0 40 0
      vertex 120 40 5
      vertex 120 40 0
    endloop
  endfacet
  facet normal 0 1 0
    outer loop
      vertex 0 40 0
      vertex 0 40 0.6
      vertex 120 40 5
    endloop
  endfacet
  facet normal -1 0 0
    outer loop
      vertex 0 0 0
      vertex 0 40 0
      vertex 0 40 0.6
    endloop
  endfacet
  facet normal -1 0 0
    outer loop
      vertex 0 0 0
      vertex 0 0 0.6
      vertex 0 40 0.6
    endloop
  endfacet
  facet normal 1 0 0
    outer loop
      vertex 120 0 0
      vertex 120 40 5
      vertex 120 40 0
    endloop
  endfacet
  facet normal 1 0 0
    outer loop
      vertex 120 0 0
      vertex 120 0 5
      vertex 120 40 5
    endloop
  endfacet
  facet normal 0 0 1
    outer loop
      vertex 0 0 0.6
      vertex 120 0 5
      vertex 120 40 5
    endloop
  endfacet
  facet normal 0 0 1
    outer loop
      vertex 0 0 0.6
      vertex 120 40 5
      vertex 0 40 0.6
    endloop
  endfacet
endsolid ramp
EOF

"${SLICER_BIN}" --slice 1 --no-check --layer-change-gcode "G92 E0" --outputdir "${BASE_OUT}" "${MODEL_FILE}"
"${SLICER_BIN}" --slice 1 --no-check --layer-change-gcode "G92 E0" --outputdir "${ZAA_OUT}" --zaa-enabled --zaa-min-z 0.03 "${MODEL_FILE}"

BASE_GCODE="${BASE_OUT}/plate_1.gcode"
ZAA_GCODE="${ZAA_OUT}/plate_1.gcode"

if [[ ! -s "${BASE_GCODE}" ]]; then
    echo "Baseline G-code was not generated: ${BASE_GCODE}" >&2
    exit 1
fi
if [[ ! -s "${ZAA_GCODE}" ]]; then
    echo "ZAA G-code was not generated: ${ZAA_GCODE}" >&2
    exit 1
fi

BASE_ZE_COUNT="$(grep -E -c '^G[123].*E[-+]?[0-9]+(\.[0-9]+)?.*Z[-+]?[0-9]+(\.[0-9]+)?' "${BASE_GCODE}" || true)"
ZAA_ZE_COUNT="$(grep -E -c '^G[123].*E[-+]?[0-9]+(\.[0-9]+)?.*Z[-+]?[0-9]+(\.[0-9]+)?' "${ZAA_GCODE}" || true)"
BASE_Z_MOVE_COUNT="$(grep -E -c '^G[0123].*Z[-+]?[0-9]+(\.[0-9]+)?' "${BASE_GCODE}" || true)"
ZAA_Z_MOVE_COUNT="$(grep -E -c '^G[0123].*Z[-+]?[0-9]+(\.[0-9]+)?' "${ZAA_GCODE}" || true)"
BASE_UNIQUE_Z="$(awk '
    /^G[0123]/ {
        for (i = 1; i <= NF; ++i) {
            if ($i ~ /^Z[-+]?[0-9]+(\.[0-9]+)?$/) {
                z = substr($i, 2);
                seen[z] = 1;
            }
        }
    }
    END {
        n = 0;
        for (k in seen) ++n;
        print n;
    }' "${BASE_GCODE}")"
ZAA_UNIQUE_Z="$(awk '
    /^G[0123]/ {
        for (i = 1; i <= NF; ++i) {
            if ($i ~ /^Z[-+]?[0-9]+(\.[0-9]+)?$/) {
                z = substr($i, 2);
                seen[z] = 1;
            }
        }
    }
    END {
        n = 0;
        for (k in seen) ++n;
        print n;
    }' "${ZAA_GCODE}")"

if cmp -s "${BASE_GCODE}" "${ZAA_GCODE}"; then
    echo "Baseline and ZAA G-code are identical; expected output change." >&2
    exit 1
fi

if [[ "${ZAA_ZE_COUNT}" -le "${BASE_ZE_COUNT}" && "${ZAA_UNIQUE_Z}" -le "${BASE_UNIQUE_Z}" && "${ZAA_Z_MOVE_COUNT}" -le "${BASE_Z_MOVE_COUNT}" ]]; then
    echo "Expected stronger variable-Z signal in ZAA output." >&2
    echo "baseline_z_extrusions=${BASE_ZE_COUNT}, zaa_z_extrusions=${ZAA_ZE_COUNT}" >&2
    echo "baseline_unique_z=${BASE_UNIQUE_Z}, zaa_unique_z=${ZAA_UNIQUE_Z}" >&2
    echo "baseline_z_moves=${BASE_Z_MOVE_COUNT}, zaa_z_moves=${ZAA_Z_MOVE_COUNT}" >&2
    echo "Sample diff (first 120 lines):" >&2
    diff -u "${BASE_GCODE}" "${ZAA_GCODE}" | sed -n '1,120p' >&2 || true
    exit 1
fi

echo "ZAA CLI smoke test passed."
echo "baseline_z_extrusions=${BASE_ZE_COUNT}, zaa_z_extrusions=${ZAA_ZE_COUNT}"
echo "baseline_unique_z=${BASE_UNIQUE_Z}, zaa_unique_z=${ZAA_UNIQUE_Z}"
echo "baseline_z_moves=${BASE_Z_MOVE_COUNT}, zaa_z_moves=${ZAA_Z_MOVE_COUNT}"
