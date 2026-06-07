from __future__ import annotations

import argparse
import glob
import json
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np


@dataclass(frozen=True)
class CalibrationReport:
    image_count: int
    image_width: int
    image_height: int
    median_reprojection_error_px: float
    max_reprojection_error_px: float
    camera_matrix: list[list[float]]
    distortion_coefficients: list[float]


def _cv2():
    try:
        import cv2  # type: ignore
    except ImportError as error:
        raise RuntimeError("OpenCV is required. Install dependencies with: python -m pip install -r requirements.txt") from error
    return cv2


def chessboard_object_points(board_size: tuple[int, int], square_size_m: float) -> np.ndarray:
    cols, rows = board_size
    points = np.zeros((rows * cols, 3), np.float32)
    points[:, :2] = np.mgrid[0:cols, 0:rows].T.reshape(-1, 2)
    points *= float(square_size_m)
    return points


def reprojection_errors(
    object_points: list[np.ndarray],
    image_points: list[np.ndarray],
    rvecs: list[np.ndarray],
    tvecs: list[np.ndarray],
    camera_matrix: np.ndarray,
    dist_coeffs: np.ndarray,
) -> list[float]:
    cv2 = _cv2()
    errors: list[float] = []
    for obj, image, rvec, tvec in zip(object_points, image_points, rvecs, tvecs, strict=True):
        projected, _ = cv2.projectPoints(obj, rvec, tvec, camera_matrix, dist_coeffs)
        delta = projected.reshape(-1, 2) - image.reshape(-1, 2)
        errors.append(float(np.median(np.linalg.norm(delta, axis=1))))
    return errors


def calibrate_points(
    object_points: list[np.ndarray],
    image_points: list[np.ndarray],
    image_size: tuple[int, int],
) -> CalibrationReport:
    cv2 = _cv2()
    if len(object_points) < 3:
        raise ValueError("at least three calibration views are required")
    width, height = image_size
    _, camera_matrix, dist_coeffs, rvecs, tvecs = cv2.calibrateCamera(
        object_points,
        image_points,
        (width, height),
        None,
        None,
    )
    errors = reprojection_errors(object_points, image_points, rvecs, tvecs, camera_matrix, dist_coeffs)
    return CalibrationReport(
        image_count=len(object_points),
        image_width=width,
        image_height=height,
        median_reprojection_error_px=round(float(np.median(errors)), 6),
        max_reprojection_error_px=round(float(np.max(errors)), 6),
        camera_matrix=np.round(camera_matrix, 6).tolist(),
        distortion_coefficients=np.round(dist_coeffs.reshape(-1), 8).tolist(),
    )


def synthetic_calibration(
    board_size: tuple[int, int] = (9, 6),
    square_size_m: float = 0.024,
    image_size: tuple[int, int] = (1280, 720),
    views: int = 16,
    noise_px: float = 0.03,
) -> CalibrationReport:
    cv2 = _cv2()
    rng = np.random.default_rng(42)
    object_template = chessboard_object_points(board_size, square_size_m)
    width, height = image_size
    camera_matrix = np.array(
        [[920.0, 0.0, width / 2.0 + 8.0], [0.0, 910.0, height / 2.0 - 5.0], [0.0, 0.0, 1.0]],
        dtype=np.float64,
    )
    dist_coeffs = np.array([-0.11, 0.032, 0.0008, -0.0003, -0.004], dtype=np.float64)

    object_points: list[np.ndarray] = []
    image_points: list[np.ndarray] = []
    for index in range(views):
        yaw = np.deg2rad(-16 + index * 2.1)
        pitch = np.deg2rad(7 * np.sin(index * 0.55))
        roll = np.deg2rad(4 * np.cos(index * 0.4))
        rvec = np.array([pitch, yaw, roll], dtype=np.float64)
        tvec = np.array(
            [
                -0.08 + 0.012 * index,
                -0.04 + 0.01 * np.sin(index),
                0.72 + 0.025 * np.cos(index * 0.6),
            ],
            dtype=np.float64,
        )
        projected, _ = cv2.projectPoints(object_template, rvec, tvec, camera_matrix, dist_coeffs)
        points = projected.astype(np.float32)
        if noise_px > 0:
            points += rng.normal(0.0, noise_px, points.shape).astype(np.float32)
        object_points.append(object_template.copy())
        image_points.append(points)

    return calibrate_points(object_points, image_points, image_size)


def calibrate_image_folder(
    image_dir: str | Path,
    board_size: tuple[int, int] = (9, 6),
    square_size_m: float = 0.024,
) -> CalibrationReport:
    cv2 = _cv2()
    image_paths = sorted(
        glob.glob(str(Path(image_dir) / "*.png"))
        + glob.glob(str(Path(image_dir) / "*.jpg"))
        + glob.glob(str(Path(image_dir) / "*.jpeg"))
    )
    if not image_paths:
        raise ValueError(f"no calibration images found in {image_dir}")

    object_template = chessboard_object_points(board_size, square_size_m)
    object_points: list[np.ndarray] = []
    image_points: list[np.ndarray] = []
    image_size: tuple[int, int] | None = None

    for image_path in image_paths:
        image = cv2.imread(image_path)
        if image is None:
            continue
        gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
        image_size = (gray.shape[1], gray.shape[0])
        found, corners = cv2.findChessboardCorners(gray, board_size)
        if not found:
            continue
        criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)
        refined = cv2.cornerSubPix(gray, corners, (11, 11), (-1, -1), criteria)
        object_points.append(object_template.copy())
        image_points.append(refined)

    if image_size is None:
        raise ValueError("no readable calibration images found")
    return calibrate_points(object_points, image_points, image_size)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run OpenCV camera calibration")
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--synthetic", action="store_true", help="Generate deterministic synthetic chessboard views")
    source.add_argument("--image-dir", help="Directory of chessboard images")
    parser.add_argument("--board-cols", type=int, default=9)
    parser.add_argument("--board-rows", type=int, default=6)
    parser.add_argument("--square-size-m", type=float, default=0.024)
    parser.add_argument("--views", type=int, default=16)
    parser.add_argument("--output", required=True)
    parser.add_argument("--max-error-px", type=float, default=0.5)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    board_size = (args.board_cols, args.board_rows)
    if args.synthetic:
        report = synthetic_calibration(board_size=board_size, square_size_m=args.square_size_m, views=args.views)
    else:
        report = calibrate_image_folder(args.image_dir, board_size=board_size, square_size_m=args.square_size_m)
    payload = asdict(report)
    Path(args.output).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(
        "views={image_count} median_error_px={median_reprojection_error_px:.4f} max_error_px={max_reprojection_error_px:.4f}".format(
            **payload
        )
    )
    if report.median_reprojection_error_px > args.max_error_px:
        raise SystemExit(3)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
