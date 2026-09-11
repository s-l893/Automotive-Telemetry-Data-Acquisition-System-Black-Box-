#!/usr/bin/env python3
"""
Validate IMU through the real SD logger / FSM pipeline.

This is NOT the imu.c unit-test (see test_imu_driver.py). This checks:

  Step 1 — real-frame path
    Rows with a normal CAN id (not 0xFFFF), 14 CSV fields, IMU columns
    present and not stuck at all-zeros while the board is alive.

  Step 2 — sentinel path
    Rows with id=0xFFFF appearing ~every 200 ms when CAN is quiet
    (but still in SYS_LOGGING, <2.5 s silence), with non-garbage IMU.

Two modes
---------
1) Live USART mirror (recommended while IMU_PIPELINE_TEST_ENABLE=1):
     python tools/test_imu_pipeline.py COM5
   Flash, reset, let boot loopback / send CAN to enter SYS_LOGGING,
   then stop CAN traffic for ~1–2 s (keep under 2.5 s).

2) Offline CSV pulled from the SD card:
     python tools/test_imu_pipeline.py --csv path/to/log_000.csv
"""

from __future__ import annotations

import argparse
import re
import statistics
import sys
import time
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

try:
    import serial
except ImportError:
    serial = None  # only required for live mode


CSV_RE = re.compile(
    r"^(\d+),(0x[0-9A-Fa-f]+),(\d+),"
    r"(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),"
    r"(-?\d+),(-?\d+),(-?\d+)\s*$"
)
IMUCSV_RE = re.compile(r"^IMUCSV\s+(.*)$")


@dataclass
class Row:
    t: int
    id_hex: str
    id_val: int
    dlc: int
    data: Tuple[int, ...]
    ax: int
    ay: int
    az: int
    raw: str


@dataclass
class Result:
    name: str
    passed: bool
    detail: str


def parse_csv_line(line: str) -> Optional[Row]:
    m = CSV_RE.match(line.strip())
    if not m:
        return None
    id_hex = m.group(2)
    return Row(
        t=int(m.group(1)),
        id_hex=id_hex,
        id_val=int(id_hex, 16),
        dlc=int(m.group(3)),
        data=tuple(int(m.group(i)) for i in range(4, 12)),
        ax=int(m.group(12)),
        ay=int(m.group(13)),
        az=int(m.group(14)),
        raw=line.strip(),
    )


def load_csv_file(path: str) -> List[Row]:
    rows: List[Row] = []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            row = parse_csv_line(line)
            if row is None:
                print(f"  WARN bad CSV line: {line!r}")
                continue
            rows.append(row)
    return rows


def capture_live(port: str, baud: int, timeout_s: float) -> List[Row]:
    if serial is None:
        raise SystemExit("pip install pyserial")
    rows: List[Row] = []
    print(f"Opening {port} @ {baud}")
    print("Reset board → enter SYS_LOGGING (CAN/loopback) → then quiet CAN for ~1–2s")
    deadline = time.time() + timeout_s
    with serial.Serial(port, baud, timeout=0.25) as ser:
        ser.reset_input_buffer()
        while time.time() < deadline:
            raw = ser.readline()
            if not raw:
                # Enough data to grade both steps?
                can_rows = [r for r in rows if r.id_val != 0xFFFF]
                sent = [r for r in rows if r.id_val == 0xFFFF]
                if len(can_rows) >= 1 and len(sent) >= 3:
                    break
                continue
            text = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            if not text:
                continue
            print(f"  {text}")
            m = IMUCSV_RE.match(text)
            payload = m.group(1) if m else text
            row = parse_csv_line(payload)
            if row:
                rows.append(row)
    return rows


def imu_looks_alive(rows: List[Row]) -> Tuple[bool, str]:
    """Reject all-zero / frozen garbage; accept post-cal near-zero residuals."""
    if not rows:
        return False, "no rows"
    axs = [r.ax for r in rows]
    ays = [r.ay for r in rows]
    azs = [r.az for r in rows]
    all_zero = all(a == 0 and b == 0 and c == 0 for a, b, c in zip(axs, ays, azs))
    if all_zero:
        return False, "IMU columns all zero"
    # If every sample identical across a long stretch, likely stale
    if len(rows) >= 5:
        frozen = all(
            (r.ax, r.ay, r.az) == (rows[0].ax, rows[0].ay, rows[0].az) for r in rows
        )
        # Flat post-cal can be nearly frozen within noise; allow small variance
        var = statistics.pvariance(axs) + statistics.pvariance(ays) + statistics.pvariance(azs)
        if frozen and var == 0 and abs(rows[0].ax) + abs(rows[0].ay) + abs(rows[0].az) > 5000:
            # Large constant vector that never moves — suspicious for post-cal
            return False, f"IMU frozen at ({rows[0].ax},{rows[0].ay},{rows[0].az})"
    means = (statistics.mean(axs), statistics.mean(ays), statistics.mean(azs))
    return True, f"n={len(rows)} mean=({means[0]:.0f},{means[1]:.0f},{means[2]:.0f})"


def step1_can_rows(rows: List[Row]) -> Result:
    can_rows = [r for r in rows if r.id_val != 0xFFFF]
    if not can_rows:
        return Result(
            "Step1 CAN+IMU row",
            False,
            "no non-sentinel rows — did you enter SYS_LOGGING / send CAN?",
        )
    # Field count already enforced by regex (14 fields)
    bad_dlc = [r for r in can_rows if r.dlc < 0 or r.dlc > 8]
    if bad_dlc:
        return Result("Step1 CAN+IMU row", False, f"bad DLC on {bad_dlc[0].raw}")
    ok, detail = imu_looks_alive(can_rows)
    sample = can_rows[0].raw
    return Result(
        "Step1 CAN+IMU row",
        ok,
        f"{detail}; example: {sample}",
    )


def step2_sentinel(rows: List[Row]) -> Result:
    sent = [r for r in rows if r.id_val == 0xFFFF]
    if len(sent) < 3:
        return Result(
            "Step2 0xFFFF sentinel",
            False,
            f"only {len(sent)} sentinel row(s) — stop CAN for ~1–2s while still LOGGING",
        )
    # Spacing ~200 ms (allow 150–350)
    dts = [b.t - a.t for a, b in zip(sent, sent[1:]) if b.t >= a.t]
    if not dts:
        return Result("Step2 0xFFFF sentinel", False, "sentinel timestamps not increasing")
    med = statistics.median(dts)
    spacing_ok = 150 <= med <= 350
    # Payload should be zeros for CAN bytes, DLC 0
    payload_ok = all(r.dlc == 0 and all(d == 0 for d in r.data) for r in sent)
    imu_ok, imu_detail = imu_looks_alive(sent)
    ok = spacing_ok and payload_ok and imu_ok
    detail = (
        f"n={len(sent)} median_dt={med:.0f}ms "
        f"payload_ok={payload_ok} imu=({imu_detail})"
    )
    return Result("Step2 0xFFFF sentinel", ok, detail)


def main() -> int:
    ap = argparse.ArgumentParser(description="IMU SD-logger pipeline test (Steps 1–2)")
    ap.add_argument("port", nargs="?", help="COM port for live IMUCSV mirror")
    ap.add_argument("--csv", help="Offline log_XXX.csv from SD card")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--timeout", type=float, default=60.0)
    args = ap.parse_args()

    if args.csv:
        print(f"Reading {args.csv}")
        rows = load_csv_file(args.csv)
    elif args.port:
        rows = capture_live(args.port, args.baud, args.timeout)
    else:
        ap.error("provide COM port or --csv path")

    print(f"\nParsed {len(rows)} CSV row(s)")
    results = [step1_can_rows(rows), step2_sentinel(rows)]

    print("\n======== RESULTS ========")
    failed = 0
    for r in results:
        mark = "PASS" if r.passed else "FAIL"
        if not r.passed:
            failed += 1
        print(f"[{mark}] {r.name}: {r.detail}")

    if failed:
        print(f"\n{failed} step(s) failed.")
        return 1
    print("\nPipeline Steps 1–2 passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
