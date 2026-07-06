// Unit tests for the MatchingEngine (matching, priority, validation).
#include <gtest/gtest.h>

#include "matching_engine.hpp"
#include "order.hpp"

namespace {

using namespace lob;

Timestamp g_ts = 0;

Order limit(OrderId id, Side side, Price price, Quantity qty,
            const std::string& sym = "TEST") {
    Order o;
    o.id        = id;
    o.symbol    = sym;
    o.side      = side;
    o.type      = OrderType::Limit;
    o.price     = price;
    o.quantity  = qty;
    o.timestamp = ++g_ts;
    return o;
}

Order market(OrderId id, Side side, Quantity qty, const std::string& sym = "TEST") {
    Order o;
    o.id        = id;
    o.symbol    = sym;
    o.side      = side;
    o.type      = OrderType::Market;
    o.price     = 0;
    o.quantity  = qty;
    o.timestamp = ++g_ts;
    return o;
}

Order cancel(OrderId id, const std::string& sym = "TEST") {
    Order o;
    o.id        = id;
    o.symbol    = sym;
    o.type      = OrderType::Cancel;
    o.timestamp = ++g_ts;
    return o;
}

// ---- Resting / no-cross behavior ----------------------------------------

TEST(Matching, NonCrossingLimitOrdersRest) {
    MatchingEngine e;
    EXPECT_EQ(e.process(limit(1, Side::Buy, 100, 10)).status, OrderStatus::Accepted);
    EXPECT_EQ(e.process(limit(2, Side::Sell, 110, 10)).status, OrderStatus::Accepted);

    const OrderBook* b = e.book("TEST");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(*b->best_bid(), 100);
    EXPECT_EQ(*b->best_ask(), 110);
    EXPECT_EQ(b->active_order_count(), 2u);
    EXPECT_EQ(e.stats().trades_executed, 0u);
}

// ---- Basic buy/sell matching --------------------------------------------

TEST(Matching, BuyLimitMatchesRestingAsk) {
    MatchingEngine e;
    e.process(limit(1, Side::Sell, 100, 10));
    ProcessResult r = e.process(limit(2, Side::Buy, 100, 10));

    EXPECT_EQ(r.status, OrderStatus::Filled);
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].buy_order_id, 2u);
    EXPECT_EQ(r.trades[0].sell_order_id, 1u);
    EXPECT_EQ(r.trades[0].execution_price, 100);
    EXPECT_EQ(r.trades[0].execution_quantity, 10u);

    const OrderBook* b = e.book("TEST");
    EXPECT_EQ(b->active_order_count(), 0u);
}

TEST(Matching, SellLimitMatchesRestingBid) {
    MatchingEngine e;
    e.process(limit(1, Side::Buy, 100, 10));
    ProcessResult r = e.process(limit(2, Side::Sell, 100, 10));

    EXPECT_EQ(r.status, OrderStatus::Filled);
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].buy_order_id, 1u);
    EXPECT_EQ(r.trades[0].sell_order_id, 2u);
    EXPECT_EQ(r.trades[0].execution_price, 100);
}

TEST(Matching, BuyDoesNotCrossAboveAskLimit) {
    MatchingEngine e;
    e.process(limit(1, Side::Sell, 105, 10));
    ProcessResult r = e.process(limit(2, Side::Buy, 100, 10));  // 100 < 105
    EXPECT_EQ(r.status, OrderStatus::Accepted);
    EXPECT_TRUE(r.trades.empty());
    EXPECT_EQ(e.book("TEST")->active_order_count(), 2u);
}

// ---- Execution price is the resting (maker) price -----------------------

TEST(Matching, ExecutionUsesRestingPrice) {
    MatchingEngine e;
    e.process(limit(1, Side::Sell, 100, 10));            // resting ask @100
    ProcessResult r = e.process(limit(2, Side::Buy, 120, 10));  // aggressive buy @120
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].execution_price, 100);  // maker price, not 120
}

// ---- Price priority ------------------------------------------------------

TEST(Matching, PricePriorityMatchesBestAskFirst) {
    MatchingEngine e;
    e.process(limit(1, Side::Sell, 105, 10));
    e.process(limit(2, Side::Sell, 100, 10));  // better ask
    e.process(limit(3, Side::Sell, 110, 10));
    ProcessResult r = e.process(limit(4, Side::Buy, 100, 10));
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].sell_order_id, 2u);          // lowest ask matched
    EXPECT_EQ(r.trades[0].execution_price, 100);
}

// ---- FIFO time priority at equal price ----------------------------------

TEST(Matching, FifoTimePriorityAtEqualPrice) {
    MatchingEngine e;
    e.process(limit(1, Side::Sell, 100, 10));  // first in queue
    e.process(limit(2, Side::Sell, 100, 10));  // second
    ProcessResult r = e.process(limit(3, Side::Buy, 100, 10));
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].sell_order_id, 1u);  // earliest arrival filled first
}

// ---- Partial fills -------------------------------------------------------

TEST(Matching, PartialFillAggressorRests) {
    MatchingEngine e;
    e.process(limit(1, Side::Sell, 100, 10));
    ProcessResult r = e.process(limit(2, Side::Buy, 100, 25));
    EXPECT_EQ(r.status, OrderStatus::PartiallyFilled);
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].execution_quantity, 10u);

    const OrderBook* b = e.book("TEST");
    ASSERT_TRUE(b->best_bid().has_value());   // remainder rests as a bid
    EXPECT_EQ(*b->best_bid(), 100);
    EXPECT_FALSE(b->best_ask().has_value());  // ask fully consumed
}

TEST(Matching, PartialFillRestingRemains) {
    MatchingEngine e;
    e.process(limit(1, Side::Sell, 100, 25));
    ProcessResult r = e.process(limit(2, Side::Buy, 100, 10));
    EXPECT_EQ(r.status, OrderStatus::Filled);
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].execution_quantity, 10u);

    const OrderBook* b = e.book("TEST");
    ASSERT_TRUE(b->best_ask().has_value());  // 15 remains resting
    EXPECT_EQ(b->front_ask()->remaining_quantity, 15u);
}

// ---- Multi-level matching ------------------------------------------------

TEST(Matching, MultiLevelSweep) {
    MatchingEngine e;
    e.process(limit(1, Side::Sell, 100, 10));
    e.process(limit(2, Side::Sell, 101, 10));
    e.process(limit(3, Side::Sell, 102, 10));
    ProcessResult r = e.process(limit(4, Side::Buy, 102, 25));

    EXPECT_EQ(r.status, OrderStatus::Filled);
    ASSERT_EQ(r.trades.size(), 3u);
    EXPECT_EQ(r.trades[0].execution_price, 100);
    EXPECT_EQ(r.trades[0].execution_quantity, 10u);
    EXPECT_EQ(r.trades[1].execution_price, 101);
    EXPECT_EQ(r.trades[1].execution_quantity, 10u);
    EXPECT_EQ(r.trades[2].execution_price, 102);
    EXPECT_EQ(r.trades[2].execution_quantity, 5u);

    const OrderBook* b = e.book("TEST");
    EXPECT_EQ(b->front_ask()->remaining_quantity, 5u);  // 5 left at 102
}

// ---- Market orders -------------------------------------------------------

TEST(Matching, MarketBuyConsumesLiquidity) {
    MatchingEngine e;
    e.process(limit(1, Side::Sell, 100, 10));
    e.process(limit(2, Side::Sell, 101, 10));
    ProcessResult r = e.process(market(3, Side::Buy, 15));
    EXPECT_EQ(r.status, OrderStatus::Filled);  // all 15 units filled across levels
    ASSERT_EQ(r.trades.size(), 2u);
    EXPECT_EQ(r.filled_quantity, 15u);
    EXPECT_EQ(e.book("TEST")->front_ask()->remaining_quantity, 5u);
}

TEST(Matching, MarketBuyFullyFilled) {
    MatchingEngine e;
    e.process(limit(1, Side::Sell, 100, 20));
    ProcessResult r = e.process(market(2, Side::Buy, 20));
    EXPECT_EQ(r.status, OrderStatus::Filled);
    EXPECT_EQ(r.filled_quantity, 20u);
}

TEST(Matching, MarketSellConsumesBids) {
    MatchingEngine e;
    e.process(limit(1, Side::Buy, 100, 10));
    e.process(limit(2, Side::Buy, 99, 10));
    ProcessResult r = e.process(market(3, Side::Sell, 12));
    ASSERT_EQ(r.trades.size(), 2u);
    EXPECT_EQ(r.trades[0].execution_price, 100);  // best bid first
    EXPECT_EQ(r.trades[1].execution_price, 99);
    EXPECT_EQ(r.filled_quantity, 12u);
}

// ---- Empty-book market orders -------------------------------------------

TEST(Matching, MarketOnEmptyBookNoTrade) {
    MatchingEngine e;
    ProcessResult r = e.process(market(1, Side::Buy, 10));
    EXPECT_EQ(r.status, OrderStatus::Accepted);  // accepted but nothing to fill
    EXPECT_TRUE(r.trades.empty());
    EXPECT_EQ(r.filled_quantity, 0u);
    EXPECT_EQ(e.book("TEST")->active_order_count(), 0u);  // market never rests
}

TEST(Matching, MarketRemainderIsDroppedNotRested) {
    MatchingEngine e;
    e.process(limit(1, Side::Sell, 100, 5));
    ProcessResult r = e.process(market(2, Side::Buy, 20));
    EXPECT_EQ(r.filled_quantity, 5u);
    EXPECT_EQ(e.book("TEST")->active_order_count(), 0u);  // 15 remainder dropped
}

// ---- Cancellation --------------------------------------------------------

TEST(Matching, CancelResting) {
    MatchingEngine e;
    e.process(limit(1, Side::Buy, 100, 10));
    ProcessResult r = e.process(cancel(1));
    EXPECT_EQ(r.status, OrderStatus::Cancelled);
    EXPECT_EQ(e.book("TEST")->active_order_count(), 0u);
    EXPECT_EQ(e.stats().cancels_succeeded, 1u);
}

TEST(Matching, CancelledOrderNoLongerMatches) {
    MatchingEngine e;
    e.process(limit(1, Side::Sell, 100, 10));
    e.process(cancel(1));
    ProcessResult r = e.process(limit(2, Side::Buy, 100, 10));
    EXPECT_TRUE(r.trades.empty());          // nothing to match against
    EXPECT_EQ(r.status, OrderStatus::Accepted);
}

TEST(Matching, CancelNonexistentRejected) {
    MatchingEngine e;
    ProcessResult r = e.process(cancel(42));
    EXPECT_EQ(r.status, OrderStatus::Rejected);
    EXPECT_EQ(e.stats().cancels_succeeded, 0u);
}

TEST(Matching, CancelWithoutSymbolStillFinds) {
    MatchingEngine e;
    e.process(limit(1, Side::Buy, 100, 10, "AAA"));
    Order c = cancel(1, "");  // no symbol routing hint
    ProcessResult r = e.process(c);
    EXPECT_EQ(r.status, OrderStatus::Cancelled);
}

// ---- Invalid orders ------------------------------------------------------

TEST(Matching, ZeroQuantityRejected) {
    MatchingEngine e;
    ProcessResult r = e.process(limit(1, Side::Buy, 100, 0));
    EXPECT_EQ(r.status, OrderStatus::Rejected);
    EXPECT_EQ(e.stats().orders_rejected, 1u);
}

TEST(Matching, NonPositiveLimitPriceRejected) {
    MatchingEngine e;
    EXPECT_EQ(e.process(limit(1, Side::Buy, 0, 10)).status, OrderStatus::Rejected);
    EXPECT_EQ(e.process(limit(2, Side::Buy, -5, 10)).status, OrderStatus::Rejected);
}

TEST(Matching, DuplicateOrderIdRejected) {
    MatchingEngine e;
    EXPECT_EQ(e.process(limit(1, Side::Buy, 100, 10)).status, OrderStatus::Accepted);
    ProcessResult r = e.process(limit(1, Side::Buy, 101, 10));  // reuse id 1
    EXPECT_EQ(r.status, OrderStatus::Rejected);
    EXPECT_EQ(e.book("TEST")->active_order_count(), 1u);  // second not added
}

TEST(Matching, DuplicateIdRejectedEvenAfterFill) {
    MatchingEngine e;
    e.process(limit(1, Side::Sell, 100, 10));
    e.process(limit(2, Side::Buy, 100, 10));  // id 1 fully filled and gone
    ProcessResult r = e.process(limit(1, Side::Sell, 100, 10));  // reuse id 1
    EXPECT_EQ(r.status, OrderStatus::Rejected);
}

// ---- Best bid/ask updates ------------------------------------------------

TEST(Matching, BestBidAskUpdateAfterFill) {
    MatchingEngine e;
    e.process(limit(1, Side::Buy, 100, 10));
    e.process(limit(2, Side::Buy, 99, 10));
    const OrderBook* b = e.book("TEST");
    EXPECT_EQ(*b->best_bid(), 100);
    e.process(limit(3, Side::Sell, 100, 10));  // takes out the 100 bid
    ASSERT_TRUE(b->best_bid().has_value());
    EXPECT_EQ(*b->best_bid(), 99);             // best bid dropped to 99
}

TEST(Matching, BestBidAskUpdateAfterCancel) {
    MatchingEngine e;
    e.process(limit(1, Side::Sell, 100, 10));
    e.process(limit(2, Side::Sell, 101, 10));
    const OrderBook* b = e.book("TEST");
    EXPECT_EQ(*b->best_ask(), 100);
    e.process(cancel(1));
    ASSERT_TRUE(b->best_ask().has_value());
    EXPECT_EQ(*b->best_ask(), 101);
}

// ---- Determinism ---------------------------------------------------------

TEST(Matching, DeterministicRepeatedRun) {
    auto run = []() {
        MatchingEngine e;
        e.process(limit(1, Side::Sell, 100, 10));
        e.process(limit(2, Side::Sell, 100, 10));
        e.process(limit(3, Side::Buy, 101, 15));
        return e.stats().volume_executed;
    };
    EXPECT_EQ(run(), run());
    EXPECT_EQ(run(), 15u);
}

// ---- Multi-symbol isolation ---------------------------------------------

TEST(Matching, SymbolsAreIsolated) {
    MatchingEngine e;
    e.process(limit(1, Side::Sell, 100, 10, "AAA"));
    ProcessResult r = e.process(limit(2, Side::Buy, 100, 10, "BBB"));
    EXPECT_TRUE(r.trades.empty());  // different symbol -> no cross
    EXPECT_EQ(r.status, OrderStatus::Accepted);
}

}  // namespace
