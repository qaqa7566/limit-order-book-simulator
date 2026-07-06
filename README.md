# Limit Order Book Simulator

A C++17 limit order book and exchange simulator implementing deterministic
price-time priority matching for limit, market, and cancel orders.

## Features
- Price-time priority matching
- Limit, market, and cancel order messages
- Partial fills and multi-level sweeping
- FIFO queues within each price level
- O(1) cancellation lookup by order ID
- Multi-symbol books
- CSV order-flow replay
- Trade output and final book-state reporting
- GoogleTest unit tests
- Benchmark executable and Python analysis tools

## Architecture
- `OrderBook`: stores bids/asks and resting orders
- `MatchingEngine`: validates, routes, matches, and records trades
- `lob_sim`: replays CSV order flow through the engine
- `benchmark_matching`: measures message throughput

## Data Structures and Complexity
- Bids: `std::map<Price, PriceLevel, std::greater<>>`
- Asks: `std::map<Price, PriceLevel, std::less<>>`
- Price level: `std::list<Order>` for FIFO ordering
- Cancel index: hash map from order ID to list iterator

Cancellation is O(1). Accessing the best bid/ask is O(1). Adding a new
price level is O(log P), where P is the number of active price levels.

## Build
...
