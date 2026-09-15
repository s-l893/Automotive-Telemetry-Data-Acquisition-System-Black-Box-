#!/usr/bin/env python3
"""
Host-side UNIT tester for the BlackBox IMU driver (imu.c only).

This does NOT test SYS_LOGGING / SD CSV / 0xFFFF sentinels.
"""

Stages
------
1. INIT  — WHO_AM_I, wake/who HAL status, fault flags from imu_init()
2. CAL   — offsets populated by imu_calibrate() (via imu_init)
3. READ  — live imu_read() samples: HAL OK, timestamps advance,
           flat-surface residual within noise bound
4. TILT  — optional interactive check that motion changes an axis

Usage
-----
  pip install pyserial
  # In CubeIDE: IMU_SELFTEST_ENABLE=1, rebuild, flash
  python tools/test_imu_driver.py COM5
  python tools/test_imu_driver.py COM5 --tilt   # also run stage 4
"""

from __future__ import annotations

import argparse
import re
import statistics
import sys
import time
from dataclasses import dataclass, field
from typing import List, Optional

try:
    import serial
except ImportError:
    print("Missing dependency: pip install pyserial", file=sys.stderr)
    sys.exit(2)


INIT_RE = re.compile(
    r"IMUTEST INIT who=0x([0-9A-Fa-f]+)\s+wake_hal=(-?\d+)\s+who_hal=(-?\d+)\s+"
    r"err=0x([0-9A-Fa-f]+)\s+f=(\d+)\s+hs=(\d+)"
)
CAL_RE = re.compile(r"IMUTEST CAL ox=(-?\d+)\s+oy=(-?\d+)\s+oz=(-?\d+)")
SAMPLE_RE = re.compile(
    r"IMUTEST SAMPLE n=(\d+)\s+t=(\d+)\s+ax=(-?\d+)\s+ay=(-?\d+)\s+az=(-?\d+)\s+"
    r"accel_hal=(-?\d+)\s+f=(\d+)\s+hs=(\d+)"
)
END_RE = re.compile(r"IMUTEST END")


@dataclass
class Sample:
    n: int
    t: int
    ax: int
    ay: int
    az: int
    accel_hal: int
    f: int
    hs: int


@dataclass
class Result:
    name: str
    passed: bool
    detail: str


@dataclass
class Capture:
    init: Optional[re.Match] = None
    cal: Optional[re.Match] = None
    samples: List[Sample] = field(default_factory=list)
    ended: bool = False


def parse_line(cap: Capture, line: str) -> None:
    m = INIT_RE.search(line)
    if m:
        cap.init = m
        return
    m = CAL_RE.search(line)
    if m:
        cap.cal = m
        return
    m = SAMPLE_RE.search(line)
    if m:
        cap.samples.append(
            Sample(
                n=int(m.group(1)),
                t=int(m.group(2)),
                ax=int(m.group(3)),
                ay=int(m.group(4)),
                az=int(m.group(5)),
                accel_hal=int(m.group(6)),
                f=int(m.group(7)),
                hs=int(m.group(8)),
            )
        )
        return
    if END_RE.search(line):
        cap.ended = True


def capture_serial(port: str, baud: int, timeout_s: float) -> Capture:
    cap = Capture()
    print(f"Opening {port} @ {baud}. Reset the board now...")
    deadline = time.time() + timeout_s
    with serial.Serial(port, baud, timeout=0.25) as ser:
        # Flush stale boot junk
        ser.reset_input_buffer()
        while time.time() < deadline:
            raw = ser.readline()
            if not raw:
                if cap.ended and len(cap.samples) >= 10:
                    break
                continue
            text = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            if text:
                print(f"  {text}")
            parse_line(cap, text)
            if cap.ended and len(cap.samples) >= 10:
                # brief drain in case more samples arrive after END
                break
    return cap


def stage1_init(cap: Capture) -> Result:
    if not cap.init:
        return Result("1 INIT (imu_init)", False, "missing IMUTEST INIT line")
    who = int(cap.init.group(1), 16)
    wake = int(cap.init.group(2))
    who_hal = int(cap.init.group(3))
    err = int(cap.init.group(4), 16)
    f = int(cap.init.group(5))
    hs = int(cap.init.group(6))
    ok = who == 0x68 and wake == 0 and who_hal == 0 and err == 0 and f == 0 and hs == 0
    detail = (
        f"who=0x{who:02X} wake_hal={wake} who_hal={who_hal} "
        f"err=0x{err:X} f={f} hs={hs}"
    )
    return Result("1 INIT (imu_init)", ok, detail)


def stage2_cal(cap: Capture) -> Result:
    if not cap.cal:
        return Result("2 CAL (imu_calibrate)", False, "missing IMUTEST CAL line")
    ox, oy, oz = (int(cap.cal.group(i)) for i in (1, 2, 3))
    # Flat board: |ox|,|oy| typically small-ish; |oz| near ~1g (~16384) is expected
    # because calibrate zeros gravity into the offset.
    ok = abs(ox) < 8000 and abs(oy) < 8000 and abs(oz) > 8000
    detail = f"ox={ox} oy={oy} oz={oz} (expect |oz|~1g after zero-cal on flat)"
    return Result("2 CAL (imu_calibrate)", ok, detail)


def stage3_read(cap: Capture, noise: int) -> Result:
    if len(cap.samples) < 10:
        return Result(
            "3 READ (imu_read)",
            False,
            f"only {len(cap.samples)} samples (need >= 10)",
        )
    bad_hal = [s for s in cap.samples if s.accel_hal != 0 or s.f != 0 or s.hs != 0]
    if bad_hal:
        s = bad_hal[0]
        return Result(
            "3 READ (imu_read)",
            False,
            f"sample n={s.n} accel_hal={s.accel_hal} f={s.f} hs={s.hs}",
        )

    ts = [s.t for s in cap.samples]
    if any(b <= a for a, b in zip(ts, ts[1:])):
        return Result("3 READ (imu_read)", False, "timestamps did not strictly increase")

    axs = [s.ax for s in cap.samples]
    ays = [s.ay for s in cap.samples]
    azs = [s.az for s in cap.samples]
    mean_ax, mean_ay, mean_az = statistics.mean(axs), statistics.mean(ays), statistics.mean(azs)
    # After zero-cal on flat board, residuals should cluster near 0
    near_zero = all(abs(m) < noise for m in (mean_ax, mean_ay, mean_az))
    detail = (
        f"n={len(cap.samples)} mean=({mean_ax:.0f},{mean_ay:.0f},{mean_az:.0f}) "
        f"noise_limit=±{noise}"
    )
    return Result("3 READ (imu_read)", near_zero, detail)


def stage4_tilt(port: str, baud: int, delta: int, timeout_s: float) -> Result:
    """Interactive: wait for a large axis excursion after user tilts the board."""
    print("\nStage 4 TILT: leave flat 2s, then tilt/rotate the board.")
    print("Watching for |ax| or |ay| or |az| mean shift >", delta)
    vals: List[Sample] = []
    deadline = time.time() + timeout_s
    with serial.Serial(port, baud, timeout=0.25) as ser:
        # Board may already have finished streaming; ask for a reset is not needed
        # if firmware stopped. For tilt we need ongoing samples — so if stream ended,
        # instruct user to re-flash with longer sample count OR we re-open and wait
        # for another boot. Simpler: require a reset for tilt stage.
        print("Reset the board again for tilt stage, then tilt after SAMPLE lines start...")
        ser.reset_input_buffer()
        while time.time() < deadline:
            raw = ser.readline()
            if not raw:
                continue
            text = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            if text:
                print(f"  {text}")
            m = SAMPLE_RE.search(text)
            if not m:
                continue
            vals.append(
                Sample(
                    n=int(m.group(1)),
                    t=int(m.group(2)),
                    ax=int(m.group(3)),
                    ay=int(m.group(4)),
                    az=int(m.group(5)),
                    accel_hal=int(m.group(6)),
                    f=int(m.group(7)),
                    hs=int(m.group(8)),
                )
            )
            if len(vals) < 8:
                continue
            # Compare early window (flat) vs late window (tilted)
            early = vals[:5]
            late = vals[-5:]
            for axis in ("ax", "ay", "az"):
                e = statistics.mean(getattr(s, axis) for s in early)
                l = statistics.mean(getattr(s, axis) for s in late)
                if abs(l - e) >= delta:
                    return Result(
                        "4 TILT (imu_read motion)",
                        True,
                        f"{axis} mean {e:.0f} -> {l:.0f} (Δ={l - e:.0f})",
                    )
    return Result(
        "4 TILT (imu_read motion)",
        False,
        f"no axis shifted by ≥{delta} within {timeout_s:.0f}s",
    )


def main() -> int:
    ap = argparse.ArgumentParser(description="Test real imu.c driver via USART2 self-test")
    ap.add_argument("port", help="Serial port (e.g. COM5 or /dev/ttyACM0)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--timeout", type=float, default=45.0, help="Capture timeout seconds")
    ap.add_argument("--noise", type=int, default=800, help="Max |mean| residual after cal")
    ap.add_argument("--tilt", action="store_true", help="Also run interactive tilt stage")
    ap.add_argument("--tilt-delta", type=int, default=1500, help="Min axis shift for tilt pass")
    ap.add_argument("--tilt-timeout", type=float, default=30.0)
    args = ap.parse_args()

    cap = capture_serial(args.port, args.baud, args.timeout)
    results = [
        stage1_init(cap),
        stage2_cal(cap),
        stage3_read(cap, args.noise),
    ]
    if args.tilt:
        results.append(stage4_tilt(args.port, args.baud, args.tilt_delta, args.tilt_timeout))

    print("\n======== RESULTS ========")
    failed = 0
    for r in results:
        mark = "PASS" if r.passed else "FAIL"
        if not r.passed:
            failed += 1
        print(f"[{mark}] {r.name}: {r.detail}")

    if failed:
        print(f"\n{failed} stage(s) failed.")
        return 1
    print("\nAll stages passed — real imu_init/calibrate/read path looks healthy.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
