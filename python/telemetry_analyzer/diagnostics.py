from __future__ import annotations

import argparse
import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable

import pandas as pd


@dataclass(frozen=True)
class ChannelSkew:
    signal: str
    samples: int
    median_period_ms: float | None
    max_jitter_ms: float | None
    first_timestamp_ns: int | None
    last_timestamp_ns: int | None


@dataclass(frozen=True)
class Anomaly:
    timestamp_ns: int
    signal: str
    value: float
    reason: str
    severity: str


def load_signals(path: str | Path) -> pd.DataFrame:
    frame = pd.read_json(path, lines=True)
    expected = {"timestamp_ns", "message", "signal", "value"}
    missing = expected.difference(frame.columns)
    if missing:
        raise ValueError(f"missing replay fields: {', '.join(sorted(missing))}")
    frame = frame.sort_values(["timestamp_ns", "message", "signal"], kind="mergesort").reset_index(drop=True)
    frame["timestamp_ns"] = frame["timestamp_ns"].astype("int64")
    frame["value"] = frame["value"].astype("float64")
    return frame


def timestamp_skew(frame: pd.DataFrame) -> list[ChannelSkew]:
    channels: list[ChannelSkew] = []
    for signal, group in frame.groupby("signal", sort=True):
        timestamps = group["timestamp_ns"].sort_values().drop_duplicates()
        if len(timestamps) < 2:
            channels.append(
                ChannelSkew(
                    signal=signal,
                    samples=int(len(timestamps)),
                    median_period_ms=None,
                    max_jitter_ms=None,
                    first_timestamp_ns=int(timestamps.iloc[0]) if len(timestamps) else None,
                    last_timestamp_ns=int(timestamps.iloc[-1]) if len(timestamps) else None,
                )
            )
            continue
        deltas_ms = timestamps.diff().dropna() / 1_000_000.0
        median = float(deltas_ms.median())
        jitter = float((deltas_ms - median).abs().max())
        channels.append(
            ChannelSkew(
                signal=signal,
                samples=int(len(timestamps)),
                median_period_ms=round(median, 6),
                max_jitter_ms=round(jitter, 6),
                first_timestamp_ns=int(timestamps.iloc[0]),
                last_timestamp_ns=int(timestamps.iloc[-1]),
            )
        )
    return channels


def _series(frame: pd.DataFrame, signal: str) -> pd.DataFrame:
    return frame.loc[frame["signal"] == signal, ["timestamp_ns", "value"]].sort_values("timestamp_ns").copy()


def _append_limit_anomalies(
    anomalies: list[Anomaly],
    rows: pd.DataFrame,
    signal: str,
    mask: pd.Series,
    reason: str,
    severity: str,
) -> None:
    for row in rows.loc[mask].itertuples(index=False):
        anomalies.append(Anomaly(int(row.timestamp_ns), signal, float(row.value), reason, severity))


def calibration_anomalies(frame: pd.DataFrame) -> list[Anomaly]:
    anomalies: list[Anomaly] = []

    checks = [
        ("steering_angle_deg", lambda values: values.abs() > 780, "steering angle outside calibrated range", "high"),
        ("steering_rate_deg_s", lambda values: values.abs() > 900, "steering rate outside calibrated range", "medium"),
        ("vehicle_speed_mps", lambda values: (values < -0.1) | (values > 85), "vehicle speed outside range", "high"),
        (
            "longitudinal_accel_mps2",
            lambda values: values.abs() > 12,
            "longitudinal acceleration outside range",
            "high",
        ),
        ("lateral_accel_mps2", lambda values: values.abs() > 12, "lateral acceleration outside range", "high"),
        ("brake_cmd_pct", lambda values: (values < 0) | (values > 100), "brake command outside 0-100 pct", "high"),
        ("reprojection_error_px", lambda values: values > 0.5, "camera reprojection error above target", "medium"),
    ]
    for signal, predicate, reason, severity in checks:
        rows = _series(frame, signal)
        if rows.empty:
            continue
        _append_limit_anomalies(anomalies, rows, signal, predicate(rows["value"]), reason, severity)

    steering = _series(frame, "steering_angle_deg")
    if len(steering) >= 2:
        jump = steering["value"].diff().abs() > 150
        _append_limit_anomalies(anomalies, steering, "steering_angle_deg", jump.fillna(False), "steering angle jump", "medium")

    speed = _series(frame, "vehicle_speed_mps")
    if len(speed) >= 2:
        speed_jump = speed["value"].diff().abs() > 8
        _append_limit_anomalies(anomalies, speed, "vehicle_speed_mps", speed_jump.fillna(False), "velocity jump", "medium")

    accel = _series(frame, "longitudinal_accel_mps2")
    if len(speed) >= 2 and not accel.empty:
        speed_diff = speed.copy()
        speed_diff["dt_s"] = speed_diff["timestamp_ns"].diff() / 1_000_000_000.0
        speed_diff["measured_accel"] = speed_diff["value"].diff() / speed_diff["dt_s"]
        speed_diff = speed_diff.dropna()
        if not speed_diff.empty:
            aligned = pd.merge_asof(
                speed_diff.sort_values("timestamp_ns"),
                accel.rename(columns={"value": "reported_accel"}).sort_values("timestamp_ns"),
                on="timestamp_ns",
                direction="nearest",
                tolerance=50_000_000,
            ).dropna()
            mismatch = (aligned["measured_accel"] - aligned["reported_accel"]).abs() > 8
            for row in aligned.loc[mismatch].itertuples(index=False):
                anomalies.append(
                    Anomaly(
                        int(row.timestamp_ns),
                        "vehicle_speed_mps",
                        float(row.value),
                        "velocity and acceleration mismatch",
                        "medium",
                    )
                )

    anomalies.sort(key=lambda item: (item.timestamp_ns, item.signal, item.reason))
    return anomalies


def build_report(frame: pd.DataFrame) -> dict[str, object]:
    anomalies = calibration_anomalies(frame)
    return {
        "samples": int(len(frame)),
        "signals": int(frame["signal"].nunique()),
        "timestamp_skew": [asdict(item) for item in timestamp_skew(frame)],
        "anomalies": [asdict(item) for item in anomalies],
        "anomaly_count": len(anomalies),
    }


def write_anomalies_csv(anomalies: Iterable[dict[str, object]], path: str | Path) -> None:
    pd.DataFrame(list(anomalies), columns=["timestamp_ns", "signal", "value", "reason", "severity"]).to_csv(path, index=False)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Analyze replayed vehicle telemetry JSON Lines")
    parser.add_argument("--input", required=True, help="Decoded replay JSONL from telemetry_replay")
    parser.add_argument("--report", required=True, help="Output diagnostics report JSON")
    parser.add_argument("--anomalies-csv", help="Optional anomaly table CSV")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    frame = load_signals(args.input)
    report = build_report(frame)
    Path(args.report).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if args.anomalies_csv:
        write_anomalies_csv(report["anomalies"], args.anomalies_csv)
    print(f"samples={report['samples']} signals={report['signals']} anomalies={report['anomaly_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
