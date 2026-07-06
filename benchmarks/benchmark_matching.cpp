// Throughput benchmark for the matching engine.
//
// Generates a large, deterministic (fixed-seed) stream of limit/market/cancel
// messages centered on a mid price and replays it through the engine, measuring
// wall-clock time and messages-per-second.
//
//   benchmark_matching [num_orders]   (default: 1,000,000)
//
// The numbers printed depend on your machine and build type; they are not
// hard-coded anywhere and should be regenerated locally.

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "matching_engine.hpp"
#include "order.hpp"

namespace {

std::vector<lob::Order> generate_orders(std::size_t count, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> action(0, 99);
    std::uniform_int_distribution<lob::Price> price_offset(-50, 50);
    std::uniform_int_distribution<int> qty_dist(1, 100);

    std::vector<lob::Order> orders;
    orders.reserve(count);

    std::vector<lob::OrderId> resting;  // candidate ids to cancel
    const lob::Price mid = 10'000;
    lob::OrderId next_id = 1;

    for (std::size_t i = 0; i < count; ++i) {
        lob::Order o;
        o.timestamp = static_cast<lob::Timestamp>(i);
        const int a = action(rng);

        if (a < 10 && !resting.empty()) {
            // ~10% cancels of a previously generated resting id.
            const std::size_t idx =
                static_cast<std::size_t>(rng() % resting.size());
            o.id     = resting[idx];
            o.symbol = "SIM";
            o.type   = lob::OrderType::Cancel;
            orders.push_back(o);
            continue;
        }

        o.id       = next_id++;
        o.symbol   = "SIM";
        o.side     = (rng() & 1u) ? lob::Side::Buy : lob::Side::Sell;
        o.quantity = static_cast<lob::Quantity>(qty_dist(rng));
        o.remaining_quantity = o.quantity;

        if (a < 20) {
            o.type  = lob::OrderType::Market;
            o.price = 0;
        } else {
            o.type  = lob::OrderType::Limit;
            o.price = mid + price_offset(rng);
            resting.push_back(o.id);  // may later be cancelled
        }
        orders.push_back(o);
    }
    return orders;
}

}  // namespace

int main(int argc, char** argv) {
    std::size_t count = 1'000'000;
    if (argc >= 2) {
        const long long requested = std::atoll(argv[1]);
        if (requested > 0) {
            count = static_cast<std::size_t>(requested);
        }
    }

    constexpr std::uint64_t kSeed = 0xC0FFEEULL;  // fixed for reproducibility
    std::cout << "Generating " << count << " orders (seed=" << kSeed << ")...\n";
    const std::vector<lob::Order> orders = generate_orders(count, kSeed);

    lob::MatchingEngine engine;

    const auto start = std::chrono::steady_clock::now();
    for (const lob::Order& o : orders) {
        engine.process(o);
    }
    const auto stop = std::chrono::steady_clock::now();

    const double seconds = std::chrono::duration<double>(stop - start).count();
    const lob::EngineStats& s = engine.stats();

    std::cout << "\n=== Benchmark results ===\n";
    std::cout << "messages processed : " << s.orders_processed << '\n';
    std::cout << "trades executed    : " << s.trades_executed << '\n';
    std::cout << "volume executed    : " << s.volume_executed << '\n';
    std::cout << "cancels succeeded  : " << s.cancels_succeeded << '\n';
    std::cout << "active resting      : " << engine.total_active_orders() << '\n';
    std::cout << "elapsed (s)        : " << seconds << '\n';
    if (seconds > 0.0) {
        const double ops = static_cast<double>(s.orders_processed) / seconds;
        std::cout << "throughput (msg/s) : " << ops << '\n';
    }
    return 0;
}
