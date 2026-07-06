#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "order.hpp"
#include "order_book.hpp"

namespace lob {

// Aggregate counters maintained across the lifetime of the engine.
struct EngineStats {
    std::uint64_t orders_processed{0};   // every message fed to process()
    std::uint64_t orders_accepted{0};    // valid new limit/market orders
    std::uint64_t orders_rejected{0};    // failed validation
    std::uint64_t cancels_succeeded{0};  // resting orders removed
    std::uint64_t trades_executed{0};
    std::uint64_t volume_executed{0};    // total matched quantity
};

// Multi-symbol matching engine implementing price-time priority for limit and
// market orders. The engine is deterministic: an identical sequence of input
// messages always yields identical trades and book state.
class MatchingEngine {
public:
    MatchingEngine() = default;

    // Validate and process a single message. Never throws on bad input: invalid
    // orders are rejected and the book is left unchanged.
    ProcessResult process(const Order& incoming);

    const EngineStats& stats() const noexcept { return stats_; }

    // Inspect a symbol's book without creating one. Returns nullptr if unknown.
    const OrderBook* book(const std::string& symbol) const;

    // All known symbols, sorted for deterministic reporting.
    std::vector<std::string> symbols() const;

    // Total resting orders across every symbol.
    std::size_t total_active_orders() const;

private:
    OrderBook&    book_for(const std::string& symbol);
    ProcessResult reject(std::string reason);
    ProcessResult do_cancel(const Order& incoming);
    ProcessResult do_match(const Order& incoming);

    std::unordered_map<std::string, OrderBook> books_;
    std::unordered_set<OrderId>                seen_ids_;  // every accepted new-order id
    TradeId                                    next_trade_id_{1};
    EngineStats                                stats_;
};

}  // namespace lob
