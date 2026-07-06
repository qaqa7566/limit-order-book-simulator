#!/usr/bin/env python3
"""Replay sample orders through the compiled C++ CLI and save the trades CSV.

Runs the `lob_sim` binary on data/sample_orders.csv, writes the resulting
trades to outputs/trades.csv, and reports the command and its outcome.
"""

import subprocess
import sys
from pathlib import Path

# Project root is the parent of this file's directory (python/).
ROOT = Path(__file__).resolve().parent.parent

CLI = ROOT / "build" / "lob_sim"
INPUT_CSV = ROOT / "data" / "sample_orders.csv"
OUTPUT_CSV = ROOT / "outputs" / "trades.csv"


def main() -> int:
    if not CLI.exists():
        print(f"error: compiled CLI not found at {CLI}", file=sys.stderr)
        print("build it first (e.g. cmake --build build)", file=sys.stderr)
        return 1

    if not INPUT_CSV.exists():
        print(f"error: input file not found at {INPUT_CSV}", file=sys.stderr)
        return 1

    OUTPUT_CSV.parent.mkdir(parents=True, exist_ok=True)

    command = [str(CLI), str(INPUT_CSV), str(OUTPUT_CSV)]
    print("command executed:")
    print("  " + " ".join(command))

    result = subprocess.run(command, capture_output=True, text=True)

    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)

    if result.returncode == 0:
        print(f"\nSUCCESS: trades saved to {OUTPUT_CSV}")
        return 0

    print(f"\nFAILED: CLI exited with code {result.returncode}", file=sys.stderr)
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
