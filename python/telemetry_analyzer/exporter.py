from __future__ import annotations

import argparse
import json
import re
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from threading import Lock


MetricKey = tuple[str, str, str]


def _label(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def _metric_name(signal: str) -> str:
    name = re.sub(r"[^a-zA-Z0-9_]", "_", signal)
    if not name or name[0].isdigit():
        name = f"signal_{name}"
    return name.lower()


class ReplayMetricStore:
    def __init__(self, replay_jsonl: Path, summary_json: Path | None = None) -> None:
        self.replay_jsonl = replay_jsonl
        self.summary_json = summary_json
        self._lock = Lock()
        self._latest: dict[MetricKey, tuple[float, int]] = {}
        self._summary: dict[str, int | float | None] = {}
        self.refresh()

    def refresh(self) -> None:
        latest: dict[MetricKey, tuple[float, int]] = {}
        if self.replay_jsonl.exists():
            with self.replay_jsonl.open("r", encoding="utf-8") as input_file:
                for line in input_file:
                    if not line.strip():
                        continue
                    payload = json.loads(line)
                    key = (str(payload.get("message", "")), str(payload["signal"]), str(payload.get("unit", "")))
                    latest[key] = (float(payload["value"]), int(payload["timestamp_ns"]))
        summary: dict[str, int | float | None] = {}
        if self.summary_json and self.summary_json.exists():
            summary = json.loads(self.summary_json.read_text(encoding="utf-8"))
        with self._lock:
            self._latest = latest
            self._summary = summary

    def prometheus(self) -> str:
        with self._lock:
            latest = dict(self._latest)
            summary = dict(self._summary)
        lines = [
            "# HELP vehicle_signal_value Latest decoded vehicle signal value",
            "# TYPE vehicle_signal_value gauge",
        ]
        for (message, signal, unit), (value, timestamp_ns) in sorted(latest.items()):
            labels = f'message="{_label(message)}",signal="{_label(signal)}",unit="{_label(unit)}"'
            lines.append(f"vehicle_signal_value{{{labels}}} {value}")
            lines.append(f"vehicle_signal_timestamp_ns{{{labels}}} {timestamp_ns}")
            lines.append(f"vehicle_{_metric_name(signal)}{{message=\"{_label(message)}\",unit=\"{_label(unit)}\"}} {value}")
        if summary:
            lines.extend(["# HELP vehicle_replay_integrity_count Replay integrity counters", "# TYPE vehicle_replay_integrity_count gauge"])
            for key in ["frames", "decoded_signals", "duplicates", "out_of_order", "dropped_frames", "unknown_messages"]:
                if key in summary and summary[key] is not None:
                    lines.append(f'vehicle_replay_integrity_count{{counter="{key}"}} {summary[key]}')
        return "\n".join(lines) + "\n"


def make_handler(store: ReplayMetricStore, refresh_seconds: float) -> type[BaseHTTPRequestHandler]:
    class MetricsHandler(BaseHTTPRequestHandler):
        last_refresh = 0.0

        def do_GET(self) -> None:  # noqa: N802
            if self.path not in {"/metrics", "/"}:
                self.send_response(404)
                self.end_headers()
                return
            now = time.monotonic()
            if now - MetricsHandler.last_refresh >= refresh_seconds:
                store.refresh()
                MetricsHandler.last_refresh = now
            payload = store.prometheus().encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; version=0.0.4; charset=utf-8")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)

        def log_message(self, format: str, *args: object) -> None:
            return

    return MetricsHandler


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Expose decoded replay telemetry as Prometheus metrics")
    parser.add_argument("--replay-jsonl", required=True)
    parser.add_argument("--summary-json")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=9108)
    parser.add_argument("--refresh-seconds", type=float, default=1.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    store = ReplayMetricStore(
        replay_jsonl=Path(args.replay_jsonl),
        summary_json=Path(args.summary_json) if args.summary_json else None,
    )
    server = ThreadingHTTPServer((args.host, args.port), make_handler(store, args.refresh_seconds))
    print(f"serving metrics on http://{args.host}:{args.port}/metrics")
    server.serve_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
