# Limit Order Book Simulator

C++17 limit order book and exchange simulator implementing deterministic
price-time priority matching for limit, market, and cancel orders.

## Highlights
- Price-time priority and FIFO execution within each price level
- Limit, market, cancel, partial-fill, and multi-level sweep handling
- Multi-symbol order books
- O(1) order lookup/cancellation by order ID
- CSV order-flow replay and executed-trade output
- GoogleTest unit tests
- Python tools for throughput and market-microstructure analysis

## Architecture
- `OrderBook`: maintains bid/ask price levels and resting orders
- `MatchingEngine`: validates messages, matches orders, and emits trades
- `lob_sim`: replays CSV order flow through the matching engine
- `benchmark_matching`: measures matching throughput

## Build and Run
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/lob_sim data/sample_orders.csv
./build/benchmark_matching

## Performance

Benchmarked locally on Apple Silicon using a CMake Release build.

| Metric | Result |
|---|---:|
| Messages processed | 1,000,000 |
| Trades executed | 777,134 |
| Executed volume | 19,823,303 |
| Successful cancels | 12,637 |
| Active resting orders after replay | 101,976 |
| Elapsed time | 0.170 seconds |
| Matching throughput | **5.88 million messages/sec** |

The benchmark uses deterministic synthetic mixed order flow and measures matching-engine performance. Results are intended for implementation comparison and are not a claim of production-exchange latency.

## Testing

```bash
ctest --test-dir build --output-on-failure
