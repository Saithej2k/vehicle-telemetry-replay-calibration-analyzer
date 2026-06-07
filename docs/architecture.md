# Architecture

The project is organized as a replay pipeline with independent analysis and visualization stages.

1. `telemetry_replay` reads a CAN CSV trace, decodes raw payloads with a DBC file, preserves original timestamps in the emitted JSON Lines stream, and checks replay integrity while it scans.
2. `telemetry_analyzer.diagnostics` reads decoded signal JSON Lines into Pandas and reports timestamp skew, limit violations, signal jumps, and cross-channel consistency anomalies.
3. `telemetry_analyzer.calibration` runs OpenCV calibration from chessboard image sequences or deterministic synthetic views, then records camera matrix, distortion coefficients, and reprojection error.
4. `telemetry_analyzer.exporter` exposes the latest decoded values and replay integrity counters as Prometheus metrics.
5. Prometheus and Grafana load the provisioning files in `prometheus/` and `grafana/` to produce the dashboard.

The C++ replay path avoids framework dependencies so it can run on developer laptops, CI hosts, and Linux machines with SocketCAN. SocketCAN output is compiled into the same binary, but it only opens on Linux hosts with an available CAN interface such as `vcan0` or a hardware adapter.
