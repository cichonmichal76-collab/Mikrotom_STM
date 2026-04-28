#!/usr/bin/env python3
"""Log original STM32 UART telemetry into SQLite.

Input frame from the working firmware:
    Iu_mA;Iv_mA;Iw_mA;pos_um;vel_mm_s
"""

from __future__ import annotations

import argparse
import datetime as dt
import os
import sqlite3
import sys
import time
from dataclasses import dataclass

try:
    import serial
except ImportError as exc:  # pragma: no cover - environment check
    raise SystemExit(
        "Missing dependency: pyserial. Install it with: python -m pip install -r requirements-pc.txt"
    ) from exc


DEFAULT_DB_PATH = os.path.join("telemetry", "mikrotom_telemetry.sqlite3")
DEFAULT_PORT = "COM6"
DEFAULT_BAUD = 460800


@dataclass(frozen=True)
class TelemetrySample:
    iu_ma: int
    iv_ma: int
    iw_ma: int
    pos_um: int
    vel_mm_s: int
    raw_line: str


def utc_now_iso() -> str:
    return dt.datetime.now(dt.UTC).isoformat(timespec="milliseconds")


def parse_frame(line: str) -> TelemetrySample | None:
    raw = line.strip()
    parts = raw.split(";")
    if len(parts) != 5:
        return None

    try:
        iu_ma, iv_ma, iw_ma, pos_um, vel_mm_s = (int(part) for part in parts)
    except ValueError:
        return None

    return TelemetrySample(
        iu_ma=iu_ma,
        iv_ma=iv_ma,
        iw_ma=iw_ma,
        pos_um=pos_um,
        vel_mm_s=vel_mm_s,
        raw_line=raw,
    )


def ensure_schema(conn: sqlite3.Connection) -> None:
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            started_at_utc TEXT NOT NULL,
            ended_at_utc TEXT,
            port TEXT NOT NULL,
            baud INTEGER NOT NULL,
            source TEXT NOT NULL,
            notes TEXT
        )
        """
    )
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS telemetry_samples (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id INTEGER NOT NULL,
            ts_utc TEXT NOT NULL,
            t_ms INTEGER NOT NULL,
            iu_ma INTEGER NOT NULL,
            iv_ma INTEGER NOT NULL,
            iw_ma INTEGER NOT NULL,
            pos_um INTEGER NOT NULL,
            vel_mm_s INTEGER NOT NULL,
            raw_line TEXT NOT NULL,
            FOREIGN KEY(session_id) REFERENCES sessions(id)
        )
        """
    )
    conn.execute(
        "CREATE INDEX IF NOT EXISTS idx_telemetry_samples_session_t_ms "
        "ON telemetry_samples(session_id, t_ms)"
    )
    conn.execute(
        "CREATE INDEX IF NOT EXISTS idx_telemetry_samples_session_pos "
        "ON telemetry_samples(session_id, pos_um)"
    )
    conn.commit()


def create_session(
    conn: sqlite3.Connection,
    port: str,
    baud: int,
    source: str,
    notes: str | None,
) -> int:
    cursor = conn.execute(
        """
        INSERT INTO sessions(started_at_utc, port, baud, source, notes)
        VALUES (?, ?, ?, ?, ?)
        """,
        (utc_now_iso(), port, baud, source, notes),
    )
    conn.commit()
    return int(cursor.lastrowid)


def finish_session(conn: sqlite3.Connection, session_id: int) -> None:
    conn.execute(
        "UPDATE sessions SET ended_at_utc = ? WHERE id = ?",
        (utc_now_iso(), session_id),
    )
    conn.commit()


def insert_sample(
    conn: sqlite3.Connection,
    session_id: int,
    started_monotonic: float,
    sample: TelemetrySample,
) -> None:
    t_ms = int((time.monotonic() - started_monotonic) * 1000)
    conn.execute(
        """
        INSERT INTO telemetry_samples(
            session_id, ts_utc, t_ms, iu_ma, iv_ma, iw_ma, pos_um, vel_mm_s, raw_line
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            session_id,
            utc_now_iso(),
            t_ms,
            sample.iu_ma,
            sample.iv_ma,
            sample.iw_ma,
            sample.pos_um,
            sample.vel_mm_s,
            sample.raw_line,
        ),
    )


def summarize_session(conn: sqlite3.Connection, session_id: int) -> dict[str, int | None]:
    row = conn.execute(
        """
        SELECT
            COUNT(*) AS count,
            MIN(pos_um) AS pos_min,
            MAX(pos_um) AS pos_max,
            MIN(vel_mm_s) AS vel_min,
            MAX(vel_mm_s) AS vel_max,
            MIN(iu_ma) AS iu_min,
            MAX(iu_ma) AS iu_max,
            MIN(iv_ma) AS iv_min,
            MAX(iv_ma) AS iv_max,
            MIN(iw_ma) AS iw_min,
            MAX(iw_ma) AS iw_max
        FROM telemetry_samples
        WHERE session_id = ?
        """,
        (session_id,),
    ).fetchone()
    keys = [description[0] for description in conn.execute("SELECT * FROM telemetry_samples LIMIT 0").description or []]
    del keys
    columns = [
        "count",
        "pos_min",
        "pos_max",
        "vel_min",
        "vel_max",
        "iu_min",
        "iu_max",
        "iv_min",
        "iv_max",
        "iw_min",
        "iw_max",
    ]
    return dict(zip(columns, row, strict=True))


def run_logger(args: argparse.Namespace) -> int:
    os.makedirs(os.path.dirname(args.db) or ".", exist_ok=True)
    conn = sqlite3.connect(args.db)
    ensure_schema(conn)
    session_id = create_session(conn, args.port, args.baud, args.source, args.notes)

    sample_count = 0
    bad_frame_count = 0
    started = time.monotonic()
    last_commit = started

    print(f"SQLite DB: {args.db}")
    print(f"Session: {session_id}")
    print(f"UART: {args.port} @ {args.baud} 8N1")
    print("Stop: Ctrl+C")

    try:
        with serial.Serial(
            port=args.port,
            baudrate=args.baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=args.timeout,
        ) as uart:
            while True:
                if args.duration_s is not None and (time.monotonic() - started) >= args.duration_s:
                    break
                if args.max_samples is not None and sample_count >= args.max_samples:
                    break

                raw_bytes = uart.readline()
                if not raw_bytes:
                    continue

                line = raw_bytes.decode("ascii", errors="ignore").strip()
                sample = parse_frame(line)
                if sample is None:
                    bad_frame_count += 1
                    continue

                insert_sample(conn, session_id, started, sample)
                sample_count += 1

                now = time.monotonic()
                if sample_count % args.batch_size == 0 or (now - last_commit) >= args.commit_interval_s:
                    conn.commit()
                    last_commit = now

                if args.echo and sample_count % args.echo_every == 0:
                    print(
                        f"{sample_count}: pos={sample.pos_um}um "
                        f"vel={sample.vel_mm_s}mm/s "
                        f"I=[{sample.iu_ma},{sample.iv_ma},{sample.iw_ma}]mA"
                    )
    except KeyboardInterrupt:
        print("\nStopped by user.")
    finally:
        conn.commit()
        finish_session(conn, session_id)
        summary = summarize_session(conn, session_id)
        conn.close()

    print("Summary:")
    print(f"  samples: {summary['count']}")
    print(f"  bad frames skipped: {bad_frame_count}")
    print(f"  pos_um: {summary['pos_min']} .. {summary['pos_max']}")
    print(f"  vel_mm_s: {summary['vel_min']} .. {summary['vel_max']}")
    print(f"  iu_ma: {summary['iu_min']} .. {summary['iu_max']}")
    print(f"  iv_ma: {summary['iv_min']} .. {summary['iv_max']}")
    print(f"  iw_ma: {summary['iw_min']} .. {summary['iw_max']}")
    return 0


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Log original Mikrotom STM UART telemetry into SQLite."
    )
    parser.add_argument("--port", default=DEFAULT_PORT, help=f"Serial port, default: {DEFAULT_PORT}")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help=f"Baudrate, default: {DEFAULT_BAUD}")
    parser.add_argument("--db", default=DEFAULT_DB_PATH, help=f"SQLite path, default: {DEFAULT_DB_PATH}")
    parser.add_argument("--duration-s", type=float, default=None, help="Optional logging duration in seconds.")
    parser.add_argument("--max-samples", type=int, default=None, help="Optional maximum number of samples.")
    parser.add_argument("--timeout", type=float, default=0.2, help="Serial read timeout in seconds.")
    parser.add_argument("--batch-size", type=int, default=200, help="Commit after this many samples.")
    parser.add_argument("--commit-interval-s", type=float, default=1.0, help="Commit at least this often.")
    parser.add_argument("--source", default="DZIALA-original-uart", help="Session source label.")
    parser.add_argument("--notes", default=None, help="Optional session notes.")
    parser.add_argument("--echo", action="store_true", help="Print periodic samples while logging.")
    parser.add_argument("--echo-every", type=int, default=200, help="Echo every N accepted samples.")
    return parser


def main() -> int:
    parser = build_arg_parser()
    args = parser.parse_args()
    return run_logger(args)


if __name__ == "__main__":
    sys.exit(main())
