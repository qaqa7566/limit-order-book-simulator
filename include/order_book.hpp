#pragma once

#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>

#include "order.hpp"

namespace lob {

// A single-symbol limit order book.
//
// This class is a pure container: it stores resting orders with price-time
// priority and exposes the primitives the matching engine needs (inspect the
// best price level, reduce the front order, cancel by id). It performs no
// matching or validation itself.
//
// Data structures (see docs/design.md for the full complexity analysis):
//   * bids_: map<Price, list<Order>, greater<>>  -> highest price first
//   * asks_: map<Price, list<Order>, less<>>     -> lowest price first
//   * each price level is a std::list preserving FIFO arrival order
//   * index_: order_id -> {side, price, list iterator} for O(1) cancellation
class OrderBook {
public:
    explicit OrderBook(std::string symbol);

    const std::string& symbol() const noexcept { return symbol_; }

    // Insert the remaining quantity of a limit order as a resting order.
    // Precondition: order.remaining_quantity > 0 and order.id is not already
    // resting in this book.
    void add_resting(const Order& order);

    // Remove a resting order by id. Returns true iff an order was removed.
    bool cancel(OrderId id);

    bool has_bids() const noexcept { return !bids_.empty(); }
    bool has_asks() const noexcept { return !asks_.empty(); }

    // Best prices, or std::nullopt when the corresponding side is empty.
    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;

    // The highest-priority resting order on each side, or nullptr when empty.
    // The pointer is valid until the next mutation of that side.
    Order* front_bid();
    Order* front_ask();
    const Order* front_bid() const;
    const Order* front_ask() const;

    // Reduce the front resting order on `resting_side` by `qty`, erasing the
    // order (and its price level) if it becomes empty. Precondition: that side
    // has a front order whose remaining_quantity >= qty.
    void reduce_front(Side resting_side, Quantity qty);

    // Number of resting orders across all price levels.
    std::size_t active_order_count() const noexcept { return index_.size(); }

private:
    using BidMap = std::map<Price, std::list<Order>, std::greater<Price>>;
    using AskMap = std::map<Price, std::list<Order>, std::less<Price>>;

    struct Location {
        Side                        side;
        Price                       price;
        std::list<Order>::iterator  it;
    };

    std::string                          symbol_;
    BidMap                               bids_;
    AskMap                               asks_;
    std::unordered_map<OrderId, Location> index_;
};

}  // namespace lob
