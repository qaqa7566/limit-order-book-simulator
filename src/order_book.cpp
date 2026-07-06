#include "order_book.hpp"

#include <iterator>
#include <utility>

namespace lob {

OrderBook::OrderBook(std::string symbol) : symbol_(std::move(symbol)) {}

void OrderBook::add_resting(const Order& order) {
    if (order.side == Side::Buy) {
        std::list<Order>& level = bids_[order.price];
        level.push_back(order);
        index_.emplace(order.id,
                       Location{Side::Buy, order.price, std::prev(level.end())});
    } else {
        std::list<Order>& level = asks_[order.price];
        level.push_back(order);
        index_.emplace(order.id,
                       Location{Side::Sell, order.price, std::prev(level.end())});
    }
}

bool OrderBook::cancel(OrderId id) {
    const auto found = index_.find(id);
    if (found == index_.end()) {
        return false;
    }
    const Location loc = found->second;
    if (loc.side == Side::Buy) {
        const auto level = bids_.find(loc.price);
        level->second.erase(loc.it);
        if (level->second.empty()) {
            bids_.erase(level);
        }
    } else {
        const auto level = asks_.find(loc.price);
        level->second.erase(loc.it);
        if (level->second.empty()) {
            asks_.erase(level);
        }
    }
    index_.erase(found);
    return true;
}

std::optional<Price> OrderBook::best_bid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const {
    if (asks_.empty()) {
        return std::nullopt;
    }
    return asks_.begin()->first;
}

Order* OrderBook::front_bid() {
    if (bids_.empty()) {
        return nullptr;
    }
    return &bids_.begin()->second.front();
}

Order* OrderBook::front_ask() {
    if (asks_.empty()) {
        return nullptr;
    }
    return &asks_.begin()->second.front();
}

const Order* OrderBook::front_bid() const {
    if (bids_.empty()) {
        return nullptr;
    }
    return &bids_.begin()->second.front();
}

const Order* OrderBook::front_ask() const {
    if (asks_.empty()) {
        return nullptr;
    }
    return &asks_.begin()->second.front();
}

void OrderBook::reduce_front(Side resting_side, Quantity qty) {
    if (resting_side == Side::Buy) {
        const auto level = bids_.begin();
        Order& front = level->second.front();
        front.remaining_quantity -= qty;
        if (front.remaining_quantity == 0) {
            index_.erase(front.id);
            level->second.pop_front();
            if (level->second.empty()) {
                bids_.erase(level);
            }
        }
    } else {
        const auto level = asks_.begin();
        Order& front = level->second.front();
        front.remaining_quantity -= qty;
        if (front.remaining_quantity == 0) {
            index_.erase(front.id);
            level->second.pop_front();
            if (level->second.empty()) {
                asks_.erase(level);
            }
        }
    }
}

}  // namespace lob
