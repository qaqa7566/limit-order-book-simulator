#pragma once

#include <cstdint>

// Core value types and enumerations shared across the order book, matching
// engine and I/O layers.
//
// Design note: prices are represented as *integers* in minor units (ticks / the
// smallest quotable increment, e.g. cents). This keeps matching exact and fully
// deterministic by avoiding floating-point comparison in the hot path. The CSV
// layer is responsible for any human-readable scaling.
namespace lob {

using OrderId   = std::uint64_t;
using Price     = std::int64_t;   // integer minor units; must be > 0 for limits
using Quantity  = std::uint64_t;  // whole units
using Timestamp = std::uint64_t;  // logical or nanosecond feed timestamp
using TradeId   = std::uint64_t;

enum class Side { Buy, Sell };

enum class OrderType { Limit, Market, Cancel };

// Outcome of feeding a single message to the matching engine.
enum class OrderStatus {
    Accepted,         // limit order accepted and rested (no fill)
    Filled,           // fully filled
    PartiallyFilled,  // partially filled; remainder rested (limit) or dropped (market)
    Cancelled,        // a cancel request removed a resting order
    Rejected          // validation failed; the book is unchanged
};

inline const char* to_string(Side s) {
    return s == Side::Buy ? "buy" : "sell";
}

inline const char* to_string(OrderType t) {
    switch (t) {
        case OrderType::Limit:  return "limit";
        case OrderType::Market: return "market";
        case OrderType::Cancel: return "cancel";
    }
    return "unknown";
}

inline const char* to_string(OrderStatus st) {
    switch (st) {
        case OrderStatus::Accepted:        return "accepted";
        case OrderStatus::Filled:          return "filled";
        case OrderStatus::PartiallyFilled: return "partially_filled";
        case OrderStatus::Cancelled:       return "cancelled";
        case OrderStatus::Rejected:        return "rejected";
    }
    return "unknown";
}

}  // namespace lob
