#include "matching_engine.hpp"

#include <algorithm>
#include <tuple>
#include <utility>

namespace lob {

OrderBook& MatchingEngine::book_for(const std::string& symbol) {
    const auto it = books_.find(symbol);
    if (it != books_.end()) {
        return it->second;
    }
    // OrderBook has no default constructor, so construct in place with the key.
    const auto inserted = books_.emplace(std::piecewise_construct,
                                         std::forward_as_tuple(symbol),
                                         std::forward_as_tuple(symbol));
    return inserted.first->second;
}

ProcessResult MatchingEngine::reject(std::string reason) {
    ++stats_.orders_rejected;
    ProcessResult result;
    result.status  = OrderStatus::Rejected;
    result.message = std::move(reason);
    return result;
}

ProcessResult MatchingEngine::process(const Order& incoming) {
    ++stats_.orders_processed;

    if (incoming.type == OrderType::Cancel) {
        return do_cancel(incoming);
    }

    // Validation for new (limit/market) orders. On any failure the book is
    // untouched and the message is rejected safely.
    if (incoming.quantity == 0) {
        return reject("zero quantity");
    }
    if (incoming.symbol.empty()) {
        return reject("missing symbol");
    }
    if (incoming.type == OrderType::Limit && incoming.price <= 0) {
        return reject("non-positive limit price");
    }
    if (incoming.price < 0) {
        return reject("negative price");
    }
    if (seen_ids_.find(incoming.id) != seen_ids_.end()) {
        return reject("duplicate order id");
    }

    seen_ids_.insert(incoming.id);
    return do_match(incoming);
}

ProcessResult MatchingEngine::do_cancel(const Order& incoming) {
    ProcessResult result;

    // A cancel references an existing order id. Route by symbol when supplied;
    // otherwise fall back to a scan. Ids are unique across books, so at most one
    // book can hold the target and the outcome is deterministic.
    bool removed = false;
    if (!incoming.symbol.empty()) {
        const auto it = books_.find(incoming.symbol);
        if (it != books_.end()) {
            removed = it->second.cancel(incoming.id);
        }
    }
    if (!removed) {
        for (auto& entry : books_) {
            if (entry.second.cancel(incoming.id)) {
                removed = true;
                break;
            }
        }
    }

    if (removed) {
        result.status = OrderStatus::Cancelled;
        ++stats_.cancels_succeeded;
    } else {
        result.status  = OrderStatus::Rejected;
        result.message = "cancel of nonexistent order";
        ++stats_.orders_rejected;
    }
    return result;
}

ProcessResult MatchingEngine::do_match(const Order& incoming) {
    ProcessResult result;
    OrderBook& book = book_for(incoming.symbol);

    Order working = incoming;
    working.remaining_quantity = working.quantity;

    const Side resting_side = (working.side == Side::Buy) ? Side::Sell : Side::Buy;

    while (working.remaining_quantity > 0) {
        Order* resting =
            (working.side == Side::Buy) ? book.front_ask() : book.front_bid();
        if (resting == nullptr) {
            break;  // no liquidity left on the opposite side
        }

        // Limit orders only cross when the price is acceptable. Market orders
        // always cross whatever is available.
        if (working.type == OrderType::Limit) {
            const bool crosses = (working.side == Side::Buy)
                                     ? resting->price <= working.price
                                     : resting->price >= working.price;
            if (!crosses) {
                break;
            }
        }

        const Quantity qty =
            std::min(working.remaining_quantity, resting->remaining_quantity);
        const Price exec_price = resting->price;  // maker sets the price

        Trade trade;
        trade.trade_id          = next_trade_id_++;
        trade.symbol            = working.symbol;
        trade.execution_price   = exec_price;
        trade.execution_quantity = qty;
        trade.timestamp         = working.timestamp;
        if (working.side == Side::Buy) {
            trade.buy_order_id  = working.id;
            trade.sell_order_id = resting->id;
        } else {
            trade.buy_order_id  = resting->id;
            trade.sell_order_id = working.id;
        }
        result.trades.push_back(std::move(trade));

        working.remaining_quantity -= qty;
        result.filled_quantity     += qty;
        book.reduce_front(resting_side, qty);  // may invalidate `resting`

        ++stats_.trades_executed;
        stats_.volume_executed += qty;
    }

    // A limit order's unfilled remainder rests; a market order's remainder is
    // dropped (no resting book presence).
    if (working.type == OrderType::Limit && working.remaining_quantity > 0) {
        book.add_resting(working);
    }

    ++stats_.orders_accepted;
    if (working.remaining_quantity == 0) {
        result.status = OrderStatus::Filled;
    } else if (result.filled_quantity > 0) {
        result.status = OrderStatus::PartiallyFilled;
    } else {
        result.status = OrderStatus::Accepted;
    }
    return result;
}

const OrderBook* MatchingEngine::book(const std::string& symbol) const {
    const auto it = books_.find(symbol);
    return it == books_.end() ? nullptr : &it->second;
}

std::vector<std::string> MatchingEngine::symbols() const {
    std::vector<std::string> out;
    out.reserve(books_.size());
    for (const auto& entry : books_) {
        out.push_back(entry.first);
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::size_t MatchingEngine::total_active_orders() const {
    std::size_t total = 0;
    for (const auto& entry : books_) {
        total += entry.second.active_order_count();
    }
    return total;
}

}  // namespace lob
