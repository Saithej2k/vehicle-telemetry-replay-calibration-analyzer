import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "python"))

from telemetry_analyzer.diagnostics import build_report, load_signals


class DiagnosticsTest(unittest.TestCase):
    def test_detects_channel_anomalies(self) -> None:
        rows = [
            {"timestamp_ns": 0, "message": "STEERING", "signal": "steering_angle_deg", "value": 10.0, "unit": "deg"},
            {"timestamp_ns": 40_000_000, "message": "STEERING", "signal": "steering_angle_deg", "value": 12.0, "unit": "deg"},
            {"timestamp_ns": 80_000_000, "message": "STEERING", "signal": "steering_angle_deg", "value": 500.0, "unit": "deg"},
            {"timestamp_ns": 10_000_000, "message": "VEHICLE_SPEED", "signal": "vehicle_speed_mps", "value": 15.0, "unit": "m_s"},
            {"timestamp_ns": 50_000_000, "message": "VEHICLE_SPEED", "signal": "vehicle_speed_mps", "value": 15.2, "unit": "m_s"},
            {"timestamp_ns": 90_000_000, "message": "VEHICLE_SPEED", "signal": "vehicle_speed_mps", "value": 2.0, "unit": "m_s"},
            {"timestamp_ns": 60_000_000, "message": "IMU_ACCEL", "signal": "longitudinal_accel_mps2", "value": 0.4, "unit": "m_s2"},
            {"timestamp_ns": 100_000_000, "message": "BRAKE", "signal": "brake_cmd_pct", "value": 125.0, "unit": "pct"},
        ]
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / "replay.jsonl"
            path.write_text("\n".join(json.dumps(row) for row in rows) + "\n", encoding="utf-8")
            report = build_report(load_signals(path))

        reasons = {item["reason"] for item in report["anomalies"]}
        self.assertIn("steering angle jump", reasons)
        self.assertIn("velocity jump", reasons)
        self.assertIn("brake command outside 0-100 pct", reasons)
        self.assertGreaterEqual(report["anomaly_count"], 3)
        self.assertGreaterEqual(len(report["timestamp_skew"]), 4)


if __name__ == "__main__":
    unittest.main()
