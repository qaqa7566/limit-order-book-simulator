#!/usr/bin/env python3
"""Summarize the trades produced by the order replay.

Reads outputs/trades.csv and prints total trades, total executed volume, and
the (volume-weighted) average execution price.
"""

import sys
from pathlib import Path

import pandas as pd

ROOT = Path(__file__).resolve().parent.parent
TRADES_CSV = ROOT / "outputs" / "trades.csv"


def main() -> int:
    if not TRADES_CSV.exists():
        print(f"error: trades file not found at {TRADES_CSV}", file=sys.stderr)
        print("run replay_orders.py first", file=sys.stderr)
        return 1

    trades = pd.read_csv(TRADES_CSV)

    total_trades = len(trades)
    total_volume = int(trades["execution_quantity"].sum()) if total_trades else 0

    if total_volume > 0:
        # Volume-weighted average execution price.
        weighted = trades["execution_price"] * trades["execution_quantity"]
        avg_price = weighted.sum() / total_volume
    else:
        avg_price = 0.0

    print("=== Trade analysis ===")
    print(f"total trades           : {total_trades}")
    print(f"total executed volume  : {total_volume}")
    print(f"average execution price: {avg_price:.2f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
