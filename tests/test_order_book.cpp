// Unit tests for the OrderBook container (storage, priority, cancellation).
#include <gtest/gtest.h>

#include "order.hpp"
#include "order_book.hpp"

namespace {

using namespace lob;

Order make_resting(OrderId id, Side side, Price price, Quantity qty) {
    Order o;
    o.id                 = id;
    o.symbol             = "TEST";
    o.side               = side;
    o.type               = OrderType::Limit;
    o.price              = price;
    o.quantity           = qty;
    o.remaining_quantity = qty;
    return o;
}

TEST(OrderBook, EmptyBookHasNoBestPrices) {
    OrderBook book("TEST");
    EXPECT_FALSE(book.has_bids());
    EXPECT_FALSE(book.has_asks());
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_EQ(book.front_bid(), nullptr);
    EXPECT_EQ(book.front_ask(), nullptr);
    EXPECT_EQ(book.active_order_count(), 0u);
}

TEST(OrderBook, BestBidIsHighestPrice) {
    OrderBook book("TEST");
    book.add_resting(make_resting(1, Side::Buy, 100, 10));
    book.add_resting(make_resting(2, Side::Buy, 105, 10));
    book.add_resting(make_resting(3, Side::Buy, 95, 10));
    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), 105);
    EXPECT_EQ(book.front_bid()->id, 2u);
}

TEST(OrderBook, BestAskIsLowestPrice) {
    OrderBook book("TEST");
    book.add_resting(make_resting(1, Side::Sell, 110, 10));
    book.add_resting(make_resting(2, Side::Sell, 108, 10));
    book.add_resting(make_resting(3, Side::Sell, 120, 10));
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), 108);
    EXPECT_EQ(book.front_ask()->id, 2u);
}

TEST(OrderBook, FifoPriorityWithinPriceLevel) {
    OrderBook book("TEST");
    book.add_resting(make_resting(1, Side::Buy, 100, 10));
    book.add_resting(make_resting(2, Side::Buy, 100, 10));
    book.add_resting(make_resting(3, Side::Buy, 100, 10));
    // First order at the level should be at the front.
    EXPECT_EQ(book.front_bid()->id, 1u);
    book.reduce_front(Side::Buy, 10);  // fully removes id 1
    EXPECT_EQ(book.front_bid()->id, 2u);
    book.reduce_front(Side::Buy, 10);
    EXPECT_EQ(book.front_bid()->id, 3u);
}

TEST(OrderBook, ReduceFrontPartialKeepsOrder) {
    OrderBook book("TEST");
    book.add_resting(make_resting(1, Side::Sell, 100, 10));
    book.reduce_front(Side::Sell, 4);
    ASSERT_NE(book.front_ask(), nullptr);
    EXPECT_EQ(book.front_ask()->id, 1u);
    EXPECT_EQ(book.front_ask()->remaining_quantity, 6u);
    EXPECT_EQ(book.active_order_count(), 1u);
}

TEST(OrderBook, ReduceFrontRemovesEmptyLevel) {
    OrderBook book("TEST");
    book.add_resting(make_resting(1, Side::Sell, 100, 10));
    book.add_resting(make_resting(2, Side::Sell, 105, 10));
    book.reduce_front(Side::Sell, 10);  // clears price level 100
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), 105);
    EXPECT_EQ(book.active_order_count(), 1u);
}

TEST(OrderBook, CancelRemovesRestingOrder) {
    OrderBook book("TEST");
    book.add_resting(make_resting(1, Side::Buy, 100, 10));
    book.add_resting(make_resting(2, Side::Buy, 100, 10));
    EXPECT_TRUE(book.cancel(1));
    EXPECT_EQ(book.active_order_count(), 1u);
    EXPECT_EQ(book.front_bid()->id, 2u);
}

TEST(OrderBook, CancelLastOrderAtLevelRemovesLevel) {
    OrderBook book("TEST");
    book.add_resting(make_resting(1, Side::Buy, 100, 10));
    book.add_resting(make_resting(2, Side::Buy, 90, 10));
    EXPECT_TRUE(book.cancel(1));
    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), 90);
}

TEST(OrderBook, CancelNonexistentReturnsFalse) {
    OrderBook book("TEST");
    book.add_resting(make_resting(1, Side::Buy, 100, 10));
    EXPECT_FALSE(book.cancel(999));
    EXPECT_EQ(book.active_order_count(), 1u);
}

TEST(OrderBook, CancelTwiceOnlyRemovesOnce) {
    OrderBook book("TEST");
    book.add_resting(make_resting(1, Side::Buy, 100, 10));
    EXPECT_TRUE(book.cancel(1));
    EXPECT_FALSE(book.cancel(1));
    EXPECT_EQ(book.active_order_count(), 0u);
}

}  // namespace
