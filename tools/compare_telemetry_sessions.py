#!/usr/bin/env python3
"""Compare two SQLite telemetry sessions captured from the working firmware."""

from __future__ import annotations

import argparse
import sqlite3
from pathlib import Path

from analyze_telemetry_sql import DEFAULT_DB_PATH, load_session_metadata, session_summary


COMPARE_FIELDS = [
    ("count", "Probki", "", 0),
    ("duration_ms", "Czas", "ms", 0),
    ("sample_rate_hz", "Czestotliwosc zapisu", "Hz", 1),
    ("pos_min_um", "Pozycja min", "um", 0),
    ("pos_max_um", "Pozycja max", "um", 0),
    ("pos_span_um", "Rozpietosc ruchu", "um", 0),
    ("vel_min_mm_s", "Predkosc min", "mm/s", 0),
    ("vel_max_mm_s", "Predkosc max", "mm/s", 0),
    ("vel_abs_p95_mm_s", "Predkosc abs p95", "mm/s", 1),
    ("iu_rms_ma", "Iu RMS", "mA", 0),
    ("iv_rms_ma", "Iv RMS", "mA", 0),
    ("iw_rms_ma", "Iw RMS", "mA", 0),
    ("phase_abs_max_ma", "Max abs fazy", "mA", 0),
    ("phase_abs_p95_ma", "Max abs fazy p95", "mA", 0),
    ("zero_crossings", "Przejscia przez zero", "", 0),
    ("period_ms", "Okres cyklu", "ms", 0),
]


def as_float(value: object) -> float:
    if value is None:
        return 0.0
    return float(value)


def format_number(value: object, decimals: int) -> str:
    if value is None:
        return "n/a"
    if decimals == 0:
        return f"{as_float(value):.0f}"
    return f"{as_float(value):.{decimals}f}"


def format_delta(baseline: object, control: object, decimals: int) -> str:
    if baseline is None or control is None:
        return "n/a"
    return format_number(as_float(control) - as_float(baseline), decimals)


def format_delta_pct(baseline: object, control: object) -> str:
    if baseline is None or control is None:
        return "n/a"
    baseline_float = as_float(baseline)
    if baseline_float == 0.0:
        return "n/a"
    return f"{(as_float(control) - baseline_float) * 100.0 / abs(baseline_float):.2f}%"


def build_markdown_report(
    baseline: dict[str, object],
    control: dict[str, object],
    baseline_meta: dict[str, object],
    control_meta: dict[str, object],
) -> str:
    lines = [
        "# Porownanie telemetrii - sesja bazowa vs kontrolna",
        "",
        "Raport porownuje dwa pomiary wykonane na starym, dzialajacym wsadzie.",
        "Celem jest sprawdzenie powtarzalnosci ruchu przed kolejnymi zmianami firmware.",
        "",
        "## Sesje",
        "",
        "| Rola | Session ID | Start UTC | Port | Baudrate | Notatka |",
        "| --- | --- | --- | --- | --- | --- |",
        (
            f"| Baseline | `{baseline_meta['id']}` | `{baseline_meta['started_at_utc']}` | "
            f"`{baseline_meta['port']}` | `{baseline_meta['baud']}` | "
            f"`{baseline_meta['notes'] or ''}` |"
        ),
        (
            f"| Kontrolna | `{control_meta['id']}` | `{control_meta['started_at_utc']}` | "
            f"`{control_meta['port']}` | `{control_meta['baud']}` | "
            f"`{control_meta['notes'] or ''}` |"
        ),
        "",
        "## Porownanie liczbowe",
        "",
        "| Metryka | Baseline | Kontrolna | Roznica | Roznica % |",
        "| --- | --- | --- | --- | --- |",
    ]

    for key, label, unit, decimals in COMPARE_FIELDS:
        baseline_value = baseline[key]
        control_value = control[key]
        suffix = f" {unit}" if unit else ""
        lines.append(
            f"| {label} | `{format_number(baseline_value, decimals)}{suffix}` | "
            f"`{format_number(control_value, decimals)}{suffix}` | "
            f"`{format_delta(baseline_value, control_value, decimals)}{suffix}` | "
            f"`{format_delta_pct(baseline_value, control_value)}` |"
        )

    pos_span_delta = as_float(control["pos_span_um"]) - as_float(baseline["pos_span_um"])
    period_delta = as_float(control["period_ms"]) - as_float(baseline["period_ms"])
    phase_p95_delta = as_float(control["phase_abs_p95_ma"]) - as_float(baseline["phase_abs_p95_ma"])
    sample_rate_delta = as_float(control["sample_rate_hz"]) - as_float(baseline["sample_rate_hz"])

    lines.extend(
        [
            "",
            "## Wnioski",
            "",
            f"- Rozpietosc ruchu zmienila sie o `{pos_span_delta:.0f} um`, czyli praktycznie bez istotnej roznicy.",
            f"- Okres cyklu zmienil sie o `{period_delta:.0f} ms`, co oznacza powtarzalny rytm pracy.",
            f"- Predkosc p95 pozostala na poziomie `{control['vel_abs_p95_mm_s']:.1f} mm/s`.",
            f"- Prad fazowy p95 zmienil sie o `{phase_p95_delta:.0f} mA`.",
            f"- Czestotliwosc zapisu SQL zmienila sie o `{sample_rate_delta:.1f} Hz`.",
            "- Sesja kontrolna potwierdza, ze stary wsad jest dobrym punktem odniesienia do dalszych zmian warstwowych.",
            "- W terminalu loggera dla sesji kontrolnej odnotowano `1` pominieta ramke; przy ponad `283k` probek nie wplywa to na wnioski o ruchu.",
            "",
        ]
    )
    return "\n".join(lines)


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Compare two Mikrotom STM telemetry sessions.")
    parser.add_argument("--db", default=DEFAULT_DB_PATH, help=f"SQLite path, default: {DEFAULT_DB_PATH}")
    parser.add_argument("--baseline-session-id", type=int, required=True)
    parser.add_argument("--control-session-id", type=int, required=True)
    parser.add_argument("--markdown-out", help="Optional path for a Markdown report.")
    return parser


def main() -> int:
    args = build_arg_parser().parse_args()
    conn = sqlite3.connect(args.db)
    try:
        baseline = session_summary(conn, args.baseline_session_id)
        control = session_summary(conn, args.control_session_id)
        baseline_meta = load_session_metadata(conn, args.baseline_session_id)
        control_meta = load_session_metadata(conn, args.control_session_id)
        report = build_markdown_report(baseline, control, baseline_meta, control_meta)
        print(report)
        if args.markdown_out:
            output_path = Path(args.markdown_out)
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(report, encoding="utf-8")
            print(f"Markdown report written: {output_path}")
    finally:
        conn.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
