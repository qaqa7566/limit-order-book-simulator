#pragma once

#include <string>
#include <vector>

#include "types.hpp"

namespace lob {

// A single order/message. For a cancel, `id` refers to the *resting* order to
// remove and the price/quantity fields are ignored.
struct Order {
    OrderId     id{0};
    std::string symbol;
    Side        side{Side::Buy};
    OrderType   type{OrderType::Limit};
    Price       price{0};              // ignored for market/cancel
    Quantity    quantity{0};           // original quantity
    Timestamp   timestamp{0};
    Quantity    remaining_quantity{0}; // set by the engine while resting/matching
};

// A completed execution between an aggressing order and a resting order.
struct Trade {
    TradeId     trade_id{0};
    OrderId     buy_order_id{0};
    OrderId     sell_order_id{0};
    std::string symbol;
    Price       execution_price{0};    // always the resting (maker) price
    Quantity    execution_quantity{0};
    Timestamp   timestamp{0};          // aggressor timestamp
};

// Result of MatchingEngine::process for one message.
struct ProcessResult {
    OrderStatus        status{OrderStatus::Rejected};
    std::string        message;         // human-readable reason when rejected
    std::vector<Trade> trades;          // executions produced by this message
    Quantity           filled_quantity{0};
};

}  // namespace lob
