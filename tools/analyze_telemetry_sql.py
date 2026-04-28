#!/usr/bin/env python3
"""Analyze SQLite telemetry captured from the working STM32 firmware."""

from __future__ import annotations

import argparse
import math
import sqlite3
import statistics
from dataclasses import dataclass


DEFAULT_DB_PATH = "telemetry/mikrotom_telemetry.sqlite3"


@dataclass(frozen=True)
class Sample:
    t_ms: int
    iu_ma: int
    iv_ma: int
    iw_ma: int
    pos_um: int
    vel_mm_s: int


def load_samples(conn: sqlite3.Connection, session_id: int) -> list[Sample]:
    rows = conn.execute(
        """
        SELECT t_ms, iu_ma, iv_ma, iw_ma, pos_um, vel_mm_s
        FROM telemetry_samples
        WHERE session_id = ?
        ORDER BY t_ms, id
        """,
        (session_id,),
    ).fetchall()
    return [Sample(*row) for row in rows]


def rms(values: list[int]) -> float:
    if not values:
        return 0.0
    return math.sqrt(sum(float(value) * float(value) for value in values) / len(values))


def percentile(values: list[int], pct: float) -> float:
    if not values:
        return 0.0
    sorted_values = sorted(values)
    idx = (len(sorted_values) - 1) * pct / 100.0
    lo = int(math.floor(idx))
    hi = int(math.ceil(idx))
    if lo == hi:
        return float(sorted_values[lo])
    return sorted_values[lo] + (sorted_values[hi] - sorted_values[lo]) * (idx - lo)


def find_zero_crossings(samples: list[Sample]) -> list[int]:
    crossings: list[int] = []
    prev = samples[0]
    for sample in samples[1:]:
        if prev.pos_um <= 0 < sample.pos_um or prev.pos_um >= 0 > sample.pos_um:
            crossings.append(sample.t_ms)
        prev = sample
    return crossings


def median_period_ms(crossings: list[int]) -> float | None:
    if len(crossings) < 3:
        return None
    half_periods = [b - a for a, b in zip(crossings, crossings[1:], strict=False) if b > a]
    if not half_periods:
        return None
    return statistics.median(half_periods) * 2.0


def find_turning_points(samples: list[Sample], min_separation_ms: int = 500) -> list[Sample]:
    if len(samples) < 3:
        return []

    points: list[Sample] = []
    last_t = -min_separation_ms
    for prev, current, nxt in zip(samples, samples[1:], samples[2:], strict=False):
        is_max = prev.pos_um < current.pos_um and current.pos_um >= nxt.pos_um
        is_min = prev.pos_um > current.pos_um and current.pos_um <= nxt.pos_um
        if (is_max or is_min) and current.t_ms - last_t >= min_separation_ms:
            points.append(current)
            last_t = current.t_ms
    return points


def session_summary(conn: sqlite3.Connection, session_id: int) -> dict[str, object]:
    samples = load_samples(conn, session_id)
    if not samples:
        raise SystemExit(f"No telemetry samples for session {session_id}")

    iu = [s.iu_ma for s in samples]
    iv = [s.iv_ma for s in samples]
    iw = [s.iw_ma for s in samples]
    pos = [s.pos_um for s in samples]
    vel = [s.vel_mm_s for s in samples]
    abs_vel = [abs(v) for v in vel]
    phase_abs_max = [max(abs(s.iu_ma), abs(s.iv_ma), abs(s.iw_ma)) for s in samples]

    duration_ms = samples[-1].t_ms - samples[0].t_ms
    crossings = find_zero_crossings(samples)
    period_ms = median_period_ms(crossings)
    turning_points = find_turning_points(samples)

    return {
        "session_id": session_id,
        "count": len(samples),
        "duration_ms": duration_ms,
        "sample_rate_hz": len(samples) * 1000.0 / duration_ms if duration_ms > 0 else 0.0,
        "pos_min_um": min(pos),
        "pos_max_um": max(pos),
        "pos_span_um": max(pos) - min(pos),
        "vel_min_mm_s": min(vel),
        "vel_max_mm_s": max(vel),
        "vel_abs_p95_mm_s": percentile(abs_vel, 95.0),
        "iu_min_ma": min(iu),
        "iu_max_ma": max(iu),
        "iv_min_ma": min(iv),
        "iv_max_ma": max(iv),
        "iw_min_ma": min(iw),
        "iw_max_ma": max(iw),
        "iu_rms_ma": rms(iu),
        "iv_rms_ma": rms(iv),
        "iw_rms_ma": rms(iw),
        "phase_abs_max_ma": max(phase_abs_max),
        "phase_abs_p95_ma": percentile(phase_abs_max, 95.0),
        "zero_crossings": len(crossings),
        "period_ms": period_ms,
        "turning_points": turning_points[:20],
    }


def print_summary(summary: dict[str, object]) -> None:
    print(f"Session: {summary['session_id']}")
    print(f"Samples: {summary['count']}")
    print(f"Duration: {summary['duration_ms']} ms")
    print(f"Sample rate: {summary['sample_rate_hz']:.1f} Hz")
    print(
        "Position: "
        f"{summary['pos_min_um']} .. {summary['pos_max_um']} um "
        f"(span {summary['pos_span_um']} um)"
    )
    print(
        "Velocity: "
        f"{summary['vel_min_mm_s']} .. {summary['vel_max_mm_s']} mm/s "
        f"(abs p95 {summary['vel_abs_p95_mm_s']:.1f} mm/s)"
    )
    print(
        "Phase current ranges: "
        f"Iu {summary['iu_min_ma']} .. {summary['iu_max_ma']} mA, "
        f"Iv {summary['iv_min_ma']} .. {summary['iv_max_ma']} mA, "
        f"Iw {summary['iw_min_ma']} .. {summary['iw_max_ma']} mA"
    )
    print(
        "Phase current RMS: "
        f"Iu {summary['iu_rms_ma']:.0f} mA, "
        f"Iv {summary['iv_rms_ma']:.0f} mA, "
        f"Iw {summary['iw_rms_ma']:.0f} mA"
    )
    print(
        "Phase absolute max: "
        f"{summary['phase_abs_max_ma']} mA "
        f"(p95 {summary['phase_abs_p95_ma']:.0f} mA)"
    )
    print(f"Zero crossings: {summary['zero_crossings']}")
    if summary["period_ms"] is not None:
        print(f"Estimated motion period: {summary['period_ms']:.0f} ms")
    else:
        print("Estimated motion period: n/a")

    turning_points = summary["turning_points"]
    if turning_points:
        print("Turning points preview:")
        for point in turning_points:
            print(f"  t={point.t_ms} ms pos={point.pos_um} um vel={point.vel_mm_s} mm/s")


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Analyze Mikrotom STM telemetry SQLite database.")
    parser.add_argument("--db", default=DEFAULT_DB_PATH, help=f"SQLite path, default: {DEFAULT_DB_PATH}")
    parser.add_argument("--session-id", type=int, default=None, help="Session id. Defaults to latest session.")
    return parser


def main() -> int:
    args = build_arg_parser().parse_args()
    conn = sqlite3.connect(args.db)
    try:
        session_id = args.session_id
        if session_id is None:
            row = conn.execute("SELECT MAX(id) FROM sessions").fetchone()
            session_id = row[0]
            if session_id is None:
                raise SystemExit("No sessions in database.")
        print_summary(session_summary(conn, session_id))
    finally:
        conn.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
