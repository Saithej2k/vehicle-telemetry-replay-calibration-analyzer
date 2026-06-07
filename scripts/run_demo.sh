#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
PYTHON_BIN="${PYTHON_BIN:-python3}"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --target telemetry_replay telemetry_cpp_tests
ctest --test-dir "${BUILD_DIR}" --output-on-failure

"${BUILD_DIR}/telemetry_replay" \
  --dbc "${ROOT_DIR}/data/dbc/demo_vehicle.dbc" \
  --input "${ROOT_DIR}/data/traces/synthetic_can.csv" \
  --output "${BUILD_DIR}/replay.jsonl" \
  --summary "${BUILD_DIR}/replay_summary.json" \
  --no-sleep

PYTHONPATH="${ROOT_DIR}/python" "${PYTHON_BIN}" -m telemetry_analyzer.diagnostics \
  --input "${BUILD_DIR}/replay.jsonl" \
  --report "${BUILD_DIR}/diagnostics_report.json" \
  --anomalies-csv "${BUILD_DIR}/diagnostics_anomalies.csv"

PYTHONPATH="${ROOT_DIR}/python" "${PYTHON_BIN}" -m telemetry_analyzer.calibration \
  --synthetic \
  --output "${BUILD_DIR}/calibration_report.json" \
  --max-error-px 0.5

printf '\nDemo artifacts:\n'
printf '  %s\n' "${BUILD_DIR}/replay.jsonl"
printf '  %s\n' "${BUILD_DIR}/replay_summary.json"
printf '  %s\n' "${BUILD_DIR}/diagnostics_report.json"
printf '  %s\n' "${BUILD_DIR}/diagnostics_anomalies.csv"
printf '  %s\n' "${BUILD_DIR}/calibration_report.json"
