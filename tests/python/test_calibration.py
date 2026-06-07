import importlib.util
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "python"))

from telemetry_analyzer.calibration import synthetic_calibration


@unittest.skipIf(importlib.util.find_spec("cv2") is None, "OpenCV is not installed")
class CalibrationTest(unittest.TestCase):
    def test_synthetic_calibration_stays_below_half_pixel(self) -> None:
        report = synthetic_calibration(views=12, noise_px=0.02)
        self.assertLess(report.median_reprojection_error_px, 0.5)
        self.assertEqual(report.image_count, 12)
        self.assertEqual(len(report.camera_matrix), 3)


if __name__ == "__main__":
    unittest.main()
