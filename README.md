# Vehicle Telemetry Replay and Sensor-Calibration Analyzer

C++20 and Python tooling for replaying CAN telemetry, checking frame integrity, diagnosing calibration anomalies, validating camera intrinsics with OpenCV, and streaming signal metrics into Grafana.

## Capabilities

- Decode raw CAN frames with DBC signal definitions.
- Preserve capture timestamps during replay, with optional real-time pacing or immediate batch mode.
- Detect duplicate, dropped, and out-of-order frames while replaying.
- Emit decoded signal JSON Lines for downstream analysis.
- Run Pandas diagnostics for timestamp skew and injected anomalies across steering, velocity, acceleration, braking, and camera calibration channels.
- Run OpenCV chessboard calibration from image folders or deterministic synthetic views and validate median reprojection error.
- Export replay values and integrity counters as Prometheus metrics for Grafana dashboards.
- Optionally transmit replayed raw frames to a Linux SocketCAN interface.

## Repository Layout

```text
include/telemetry/       C++ public interfaces
src/                     C++ replay, DBC parsing, decoding, SocketCAN, CLI
python/telemetry_analyzer/  Diagnostics, calibration, and metrics exporter
data/dbc/                Demo DBC file
data/traces/             Demo CAN trace with known integrity and calibration issues
grafana/                 Dashboard provisioning
prometheus/              Scrape configuration
tests/                   C++ and Python tests
scripts/                 End-to-end demo runner
```

## Quick Start

Build and run the replay engine:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target telemetry_replay telemetry_cpp_tests
ctest --test-dir build --output-on-failure

./build/telemetry_replay \
  --dbc data/dbc/demo_vehicle.dbc \
  --input data/traces/synthetic_can.csv \
  --output build/replay.jsonl \
  --summary build/replay_summary.json \
  --no-sleep
```

Install Python dependencies and run diagnostics:

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt

PYTHONPATH=python python -m telemetry_analyzer.diagnostics \
  --input build/replay.jsonl \
  --report build/diagnostics_report.json \
  --anomalies-csv build/diagnostics_anomalies.csv
```

Validate camera calibration with deterministic synthetic chessboard views:

```bash
PYTHONPATH=python python -m telemetry_analyzer.calibration \
  --synthetic \
  --output build/calibration_report.json \
  --max-error-px 0.5
```

Or run the full local path:

```bash
PYTHON_BIN=.venv/bin/python scripts/run_demo.sh
```

## Grafana

Generate replay artifacts, start the metrics exporter, then launch Prometheus and Grafana:

```bash
PYTHONPATH=python python -m telemetry_analyzer.exporter \
  --replay-jsonl build/replay.jsonl \
  --summary-json build/replay_summary.json \
  --port 9108
```

In another shell:

```bash
docker compose up
```

Grafana is available at `http://localhost:3000` with `admin` / `admin`. The dashboard is provisioned under `Vehicle Telemetry`.

## SocketCAN Replay

On Linux, create a virtual CAN interface and pass it to the replay binary:

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

./build/telemetry_replay \
  --dbc data/dbc/demo_vehicle.dbc \
  --input data/traces/synthetic_can.csv \
  --socketcan vcan0 \
  --no-sleep
```

## Testing

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

PYTHONPATH=python python -m unittest discover -s tests/python
```

The OpenCV calibration test is skipped when OpenCV is not installed.
