# Limit Order Book Simulator

A C++17 limit order book simulator that implements price-time priority matching for market and limit orders. The project includes deterministic unit tests, CSV-based order-flow replay, trade output, a synthetic throughput benchmark, and lightweight Python tools for replay and execution analysis.

Built as a systems-focused project to explore the core mechanics behind exchange matching engines. It is **not** production trading software and should not be used for live trading.

## Highlights

* Price-time priority and FIFO execution within each price level
* Limit, market, cancel, partial-fill, and multi-level sweep handling
* Multi-symbol order books
* Efficient order lookup and cancellation by order ID
* CSV order-flow replay and executed-trade output
* Deterministic GoogleTest unit tests
* C++ throughput benchmark with synthetic mixed order flow
* Python tools for replaying sample data and analyzing executions

## Architecture

```text
CSV Order Flow
      |
      v
+------------------+
|     lob_sim      |  Parses messages and replays order flow
+------------------+
      |
      v
+------------------+
|  MatchingEngine  |  Validates messages, matches orders, emits trades
+------------------+
      |
      v
+------------------+
|    OrderBook      |  Maintains bid/ask price levels and resting orders
+------------------+
      |
      +----------------------------+
      |                            |
      v                            v
Executed Trades CSV          Final Book State / Metrics
```

### Main components

* `OrderBook`: maintains bid and ask price levels, resting orders, and order-ID lookup.
* `MatchingEngine`: validates incoming messages, applies price-time priority, performs matching, and emits trades.
* `lob_sim`: command-line replay tool that processes CSV order flow and writes executed trades.
* `benchmark_matching`: generates deterministic synthetic order flow and measures matching-engine throughput.
* `python/replay_orders.py`: runs the compiled simulator against sample order data.
* `python/analyze_results.py`: summarizes generated trade output.

## Repository Structure

```text
.
├── benchmarks/
│   └── benchmark_matching.cpp
├── data/
│   └── sample_orders.csv
├── include/
│   ├── matching_engine.hpp
│   ├── order.hpp
│   ├── order_book.hpp
│   └── types.hpp
├── outputs/                  # Generated locally; ignored by Git
├── python/
│   ├── analyze_results.py
│   ├── replay_orders.py
│   └── requirements.txt
├── src/
│   ├── main.cpp
│   ├── matching_engine.cpp
│   └── order_book.cpp
├── tests/
│   ├── test_matching_engine.cpp
│   └── test_order_book.cpp
├── CMakeLists.txt
└── README.md
```

## Supported Order Types

| Message Type | Description                                                    |
| ------------ | -------------------------------------------------------------- |
| `limit`      | Rests in the book if not immediately executable                |
| `market`     | Consumes available liquidity until filled or the book is empty |
| `cancel`     | Removes the remaining quantity of an active resting order      |

Supported sides:

```text
buy
sell
```

## Matching Rules

The simulator follows standard price-time priority:

1. **Best price executes first**

   * Highest bid has priority among buy orders.
   * Lowest ask has priority among sell orders.

2. **FIFO within the same price level**

   * Earlier resting orders execute before later orders at the same price.

3. **Limit orders**

   * A buy limit order matches asks priced at or below its limit.
   * A sell limit order matches bids priced at or above its limit.
   * Any remaining quantity rests in the book.

4. **Market orders**

   * A market order consumes available liquidity until filled or no matching orders remain.
   * Unfilled market quantity does not rest in the order book.

5. **Cancellation**

   * A cancel request removes remaining quantity only for a valid active order ID.

## Build and Run

### Prerequisites

* CMake 3.16+
* C++17-compatible compiler
* Python 3.9+ for the analysis scripts
* `pandas` for Python analysis

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Run Unit Tests

```bash
ctest --test-dir build --output-on-failure
```

### Run the Simulator on Sample Data

```bash
./build/lob_sim data/sample_orders.csv outputs/trades.csv
```

The simulator prints a summary including:

* Rows read
* Accepted and rejected orders
* Successful cancellations
* Trades executed
* Total executed volume
* Active resting orders
* Final best bid and best ask by symbol
* Processing time and approximate throughput

### Run the Python Replay Tool

Install the required package:

```bash
python3 -m pip install -r python/requirements.txt
```

Replay the included sample order flow:

```bash
python3 python/replay_orders.py
```

Analyze resulting trades:

```bash
python3 python/analyze_results.py
```

Example output:

```text
=== Trade analysis ===
total trades           : 6
total executed volume  : 450
average execution price: 10061.11
```

## Input CSV Format

The simulator reads order-flow messages in CSV format:

```text
order_id,symbol,side,order_type,price,quantity,timestamp
```

Example:

```csv
1,AAPL,buy,limit,10000,100,1
2,AAPL,sell,limit,10050,50,2
3,AAPL,buy,market,0,25,3
```

### Field Definitions

| Field        | Description                                                 |
| ------------ | ----------------------------------------------------------- |
| `order_id`   | Unique identifier for the order or cancellation target      |
| `symbol`     | Instrument symbol, such as `AAPL`                           |
| `side`       | `buy` or `sell`                                             |
| `order_type` | `limit`, `market`, or `cancel`                              |
| `price`      | Limit price; market orders use `0`                          |
| `quantity`   | Requested quantity                                          |
| `timestamp`  | Monotonic integer timestamp used for deterministic ordering |

Invalid messages are safely rejected. Examples include:

* Zero or negative quantity
* Negative limit price
* Duplicate active order IDs
* Cancelling an order that does not exist
* Invalid side or order type

## Output Trade CSV Format

Executed trades are written using the following columns:

```text
trade_id,buy_order_id,sell_order_id,symbol,execution_price,execution_quantity,timestamp
```

Example:

```csv
1,1,2,AAPL,10050,50,2
```

## Benchmark

Run the benchmark after building:

```bash
./build/benchmark_matching
```

The benchmark uses deterministic synthetic mixed order flow to measure matching-engine performance. It reports processed messages, trades, volume, cancellations, active resting orders, elapsed time, and approximate throughput.

### Local Benchmark Result

Benchmarked locally on Apple Silicon using a CMake Release build:

| Metric                             |                    Result |
| ---------------------------------- | ------------------------: |
| Messages processed                 |                 1,000,000 |
| Trades executed                    |                   777,134 |
| Executed volume                    |                19,823,303 |
| Successful cancels                 |                    12,637 |
| Active resting orders after replay |                   101,976 |
| Elapsed time                       |             0.170 seconds |
| Matching throughput                | 5.88 million messages/sec |

These results are intended for implementation comparison on this machine and workload only. They are not a claim of production-exchange latency or real-market performance.

## Testing

The test suite covers:

* Buy and sell limit-order matching
* Best-price priority
* FIFO time priority at equal prices
* Market-order execution
* Partial fills
* Multi-level matching and liquidity sweeps
* Order cancellation
* Invalid-order rejection
* Duplicate order IDs
* Empty-book market orders
* Best bid and ask updates after fills and cancellations

Run all tests:

```bash
ctest --test-dir build --output-on-failure
```

## Design Notes

### Data Structures

The order book maintains:

* Ordered bid price levels, sorted from highest to lowest
* Ordered ask price levels, sorted from lowest to highest
* FIFO queues of resting orders within each price level
* An order-ID index for cancellation lookup

This design prioritizes correctness, deterministic matching behavior, and understandable implementation over exchange-grade memory layout or lock-free concurrency.

### Complexity Tradeoffs

* Accessing the best bid or ask is efficient through ordered price levels.
* FIFO execution is preserved using a queue-like structure at each price level.
* Cancellation uses an order-ID index to avoid scanning the entire order book.
* Matching can span multiple levels for market orders or aggressively priced limit orders.

The project does not currently implement low-latency exchange optimizations such as lock-free queues, memory pooling, sharded books, kernel-bypass networking, or hardware acceleration.

## Limitations and Future Improvements

Potential extensions include:

* Replace map-based price levels with more specialized structures for denser price grids.
* Add order modification and replace messages.
* Add iceberg, stop, or fill-or-kill order types.
* Add detailed latency instrumentation per message.
* Add historical market-data adapters.
* Add a Python notebook or visualization layer for order-book depth and execution analysis.
* Add concurrent multi-symbol processing with deterministic partitioning.
* Add randomized property-based testing and fuzzing.
* Add a simple market-making or execution-strategy simulator on top of the matching engine.

## Resume Description

> Built a C++ limit-order-book simulator with price-time matching, cancellation, partial fills, and deterministic tests; added Python tools to replay order flow and analyze execution quality.

## License

This project is released under the license included in [`LICENSE`](LICENSE).
