#!/usr/bin/env python3
"""Capture DBG59 lines from USART2 into debug-59d642.log (NDJSON)."""
import argparse
import json
import re
import sys
from pathlib import Path

try:
    import serial
except ImportError:
    print("pip install pyserial", file=sys.stderr)
    sys.exit(1)

ROOT = Path(__file__).resolve().parents[1]
LOG = ROOT / "debug-59d642.log"
PAT = re.compile(r"DBG59\s+(\{.*\})")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("port", help="COM port, e.g. COM3")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    print(f"Listening on {args.port} @ {args.baud}; writing {LOG}")
    print("Reset the board now. Ctrl+C to stop.")
    with serial.Serial(args.port, args.baud, timeout=0.5) as ser, LOG.open("a", encoding="utf-8") as out:
        while True:
            raw = ser.readline()
            if not raw:
                continue
            text = raw.decode("utf-8", errors="replace").strip()
            print(text)
            m = PAT.search(text)
            if m:
                try:
                    obj = json.loads(m.group(1))
                except json.JSONDecodeError:
                    obj = {"sessionId": "59d642", "message": "parse_fail", "raw": text}
                obj.setdefault("sessionId", "59d642")
                out.write(json.dumps(obj, separators=(",", ":")) + "\n")
                out.flush()


if __name__ == "__main__":
    main()
