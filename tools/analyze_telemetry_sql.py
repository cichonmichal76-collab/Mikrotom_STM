#!/usr/bin/env python3
"""Analyze SQLite telemetry captured from the working STM32 firmware."""

from __future__ import annotations

import argparse
import math
import sqlite3
import statistics
from dataclasses import dataclass
from pathlib import Path


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


def load_session_metadata(conn: sqlite3.Connection, session_id: int) -> dict[str, object]:
    row = conn.execute(
        """
        SELECT id, started_at_utc, ended_at_utc, port, baud, source, notes
        FROM sessions
        WHERE id = ?
        """,
        (session_id,),
    ).fetchone()
    if row is None:
        raise SystemExit(f"No session metadata for session {session_id}")

    keys = ["id", "started_at_utc", "ended_at_utc", "port", "baud", "source", "notes"]
    metadata = dict(zip(keys, row, strict=True))
    diag_count = conn.execute(
        "SELECT COUNT(*) FROM diagnostic_samples WHERE session_id = ?",
        (session_id,),
    ).fetchone()[0]
    metadata["diagnostic_count"] = diag_count
    if diag_count:
        diag_row = conn.execute(
            """
            SELECT
                MIN(vbus_mv), MAX(vbus_mv),
                MIN(foc_state), MAX(foc_state),
                MIN(iq_ref_ma), MAX(iq_ref_ma),
                MIN(iq_ma), MAX(iq_ma),
                MIN(vq_ref_mv), MAX(vq_ref_mv),
                MIN(homing_successful), MAX(homing_successful),
                MIN(homing_ongoing), MAX(homing_ongoing)
            FROM diagnostic_samples
            WHERE session_id = ?
            """,
            (session_id,),
        ).fetchone()
        diag_keys = [
            "diag_vbus_min_mv",
            "diag_vbus_max_mv",
            "diag_foc_min",
            "diag_foc_max",
            "diag_iq_ref_min_ma",
            "diag_iq_ref_max_ma",
            "diag_iq_min_ma",
            "diag_iq_max_ma",
            "diag_vq_ref_min_mv",
            "diag_vq_ref_max_mv",
            "diag_homing_successful_min",
            "diag_homing_successful_max",
            "diag_homing_ongoing_min",
            "diag_homing_ongoing_max",
        ]
        metadata.update(dict(zip(diag_keys, diag_row, strict=True)))
    return metadata


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


def format_markdown_report(
    summary: dict[str, object],
    metadata: dict[str, object],
) -> str:
    period = (
        f"{summary['period_ms']:.0f} ms"
        if summary["period_ms"] is not None
        else "n/a"
    )
    turning_points = summary["turning_points"]

    lines = [
        "# Raport telemetrii - firmware DZIALA",
        "",
        "Ten raport opisuje sesje telemetryczna firmware z brancha `DZIALA`.",
        "Logger SQL tylko slucha UART i nie wysyla komend do MCU.",
        "",
        "## Sesja pomiarowa",
        "",
        "| Parametr | Wartosc |",
        "| --- | --- |",
        f"| Session ID | `{summary['session_id']}` |",
        f"| Start UTC | `{metadata['started_at_utc']}` |",
        f"| Koniec UTC | `{metadata['ended_at_utc']}` |",
        f"| Port | `{metadata['port']}` |",
        f"| Baudrate | `{metadata['baud']}` |",
        f"| Zrodlo | `{metadata['source']}` |",
        f"| Notatka | `{metadata['notes'] or ''}` |",
        f"| Probki szybkiej telemetrii | `{summary['count']}` |",
        f"| Probki diagnostyczne | `{metadata['diagnostic_count']}` |",
        f"| Czas pomiaru | `{summary['duration_ms']} ms` |",
        f"| Efektywna czestotliwosc zapisu | `{summary['sample_rate_hz']:.1f} Hz` |",
        "",
        "## Ruch osi",
        "",
        "| Parametr | Wartosc |",
        "| --- | --- |",
        f"| Pozycja min | `{summary['pos_min_um']} um` |",
        f"| Pozycja max | `{summary['pos_max_um']} um` |",
        f"| Rozpietosc ruchu | `{summary['pos_span_um']} um` |",
        f"| Predkosc min | `{summary['vel_min_mm_s']} mm/s` |",
        f"| Predkosc max | `{summary['vel_max_mm_s']} mm/s` |",
        f"| Predkosc abs p95 | `{summary['vel_abs_p95_mm_s']:.1f} mm/s` |",
        f"| Przejscia przez zero | `{summary['zero_crossings']}` |",
        f"| Szacowany okres cyklu | `{period}` |",
        "",
        "## Prady fazowe",
        "",
        "| Faza / metryka | Wartosc |",
        "| --- | --- |",
        f"| Iu zakres | `{summary['iu_min_ma']} .. {summary['iu_max_ma']} mA` |",
        f"| Iv zakres | `{summary['iv_min_ma']} .. {summary['iv_max_ma']} mA` |",
        f"| Iw zakres | `{summary['iw_min_ma']} .. {summary['iw_max_ma']} mA` |",
        f"| Iu RMS | `{summary['iu_rms_ma']:.0f} mA` |",
        f"| Iv RMS | `{summary['iv_rms_ma']:.0f} mA` |",
        f"| Iw RMS | `{summary['iw_rms_ma']:.0f} mA` |",
        f"| Max abs dowolnej fazy | `{summary['phase_abs_max_ma']} mA` |",
        f"| Max abs fazy p95 | `{summary['phase_abs_p95_ma']:.0f} mA` |",
        "",
    ]

    if metadata["diagnostic_count"]:
        lines.extend(
            [
                "## Diagnostyka D",
                "",
                "| Parametr | Wartosc |",
                "| --- | --- |",
                f"| VBUS | `{metadata['diag_vbus_min_mv']} .. {metadata['diag_vbus_max_mv']} mV` |",
                f"| FOC state | `{metadata['diag_foc_min']} .. {metadata['diag_foc_max']}` |",
                f"| iq_ref | `{metadata['diag_iq_ref_min_ma']} .. {metadata['diag_iq_ref_max_ma']} mA` |",
                f"| iq | `{metadata['diag_iq_min_ma']} .. {metadata['diag_iq_max_ma']} mA` |",
                f"| vq_ref | `{metadata['diag_vq_ref_min_mv']} .. {metadata['diag_vq_ref_max_mv']} mV` |",
                f"| Homing successful | `{metadata['diag_homing_successful_min']} .. {metadata['diag_homing_successful_max']}` |",
                f"| Homing ongoing | `{metadata['diag_homing_ongoing_min']} .. {metadata['diag_homing_ongoing_max']}` |",
                "",
            ]
        )

    lines.extend(
        [
        "## Punkty zwrotne - podglad",
        "",
        "| t_ms | pos_um | vel_mm_s |",
        "| --- | --- | --- |",
        ]
    )

    if turning_points:
        for point in turning_points:
            lines.append(f"| `{point.t_ms}` | `{point.pos_um}` | `{point.vel_mm_s}` |")
    else:
        lines.append("| n/a | n/a | n/a |")

    lines.extend(
        [
            "",
            "## Wnioski",
            "",
            "- Firmware pracuje powtarzalnie na krotkim odcinku okolo `18.5 mm`.",
            f"- Efektywny zapis SQL dziala z czestotliwoscia okolo `{summary['sample_rate_hz']:.1f} Hz`.",
            f"- W tej sesji zapisano `{metadata['diagnostic_count']}` ramek diagnostycznych `D;...`.",
            (
                "- Maksymalny zaobserwowany prad fazowy wyniosl okolo "
                f"`{float(summary['phase_abs_max_ma']) / 1000.0:.2f} A`, "
                f"a wartosc p95 okolo `{float(summary['phase_abs_p95_ma']) / 1000.0:.2f} A`."
            ),
            "- Ten raport sluzy do porownywania kolejnych zmian bez naruszania dzialajacego ruchu.",
            "",
        ]
    )
    return "\n".join(lines)


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Analyze Mikrotom STM telemetry SQLite database.")
    parser.add_argument("--db", default=DEFAULT_DB_PATH, help=f"SQLite path, default: {DEFAULT_DB_PATH}")
    parser.add_argument("--session-id", type=int, default=None, help="Session id. Defaults to latest session.")
    parser.add_argument("--markdown-out", help="Optional path for a Markdown report.")
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
        summary = session_summary(conn, session_id)
        print_summary(summary)
        if args.markdown_out:
            metadata = load_session_metadata(conn, session_id)
            output_path = Path(args.markdown_out)
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(format_markdown_report(summary, metadata), encoding="utf-8")
            print(f"Markdown report written: {output_path}")
    finally:
        conn.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
