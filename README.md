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
