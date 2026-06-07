import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "python"))

from telemetry_analyzer.exporter import ReplayMetricStore


class ExporterTest(unittest.TestCase):
    def test_prometheus_output_contains_latest_signal_and_integrity_counter(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            replay = Path(tmpdir) / "replay.jsonl"
            summary = Path(tmpdir) / "summary.json"
            replay.write_text(
                "\n".join(
                    [
                        json.dumps(
                            {
                                "timestamp_ns": 0,
                                "message": "STEERING",
                                "signal": "steering_angle_deg",
                                "value": 10.0,
                                "unit": "deg",
                            }
                        ),
                        json.dumps(
                            {
                                "timestamp_ns": 100,
                                "message": "STEERING",
                                "signal": "steering_angle_deg",
                                "value": 12.5,
                                "unit": "deg",
                            }
                        ),
                    ]
                )
                + "\n",
                encoding="utf-8",
            )
            summary.write_text(json.dumps({"frames": 2, "duplicates": 0}), encoding="utf-8")
            metrics = ReplayMetricStore(replay, summary).prometheus()

        self.assertIn('vehicle_signal_value{message="STEERING",signal="steering_angle_deg",unit="deg"} 12.5', metrics)
        self.assertIn('vehicle_replay_integrity_count{counter="frames"} 2', metrics)


if __name__ == "__main__":
    unittest.main()
