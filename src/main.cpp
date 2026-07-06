// Command-line driver for the limit order book simulator.
//
//   lob_sim <input.csv> [output_trades.csv]
//
// Reads an order-flow CSV, replays it through the matching engine in order,
// writes executed trades to CSV, and prints a summary of the final book state
// and processing throughput. All matching logic lives in the engine/book; this
// file only handles I/O, argument parsing and reporting.

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "matching_engine.hpp"
#include "order.hpp"
#include "types.hpp"

namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string trim(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ',')) {
        fields.push_back(trim(field));
    }
    return fields;
}

std::optional<lob::Side> parse_side(const std::string& s) {
    const std::string v = to_lower(s);
    if (v == "buy" || v == "b" || v == "bid") return lob::Side::Buy;
    if (v == "sell" || v == "s" || v == "ask") return lob::Side::Sell;
    return std::nullopt;
}

std::optional<lob::OrderType> parse_type(const std::string& s) {
    const std::string v = to_lower(s);
    if (v == "limit") return lob::OrderType::Limit;
    if (v == "market") return lob::OrderType::Market;
    if (v == "cancel") return lob::OrderType::Cancel;
    return std::nullopt;
}

// Parse one data row into an Order. Returns nullopt on malformed input so the
// caller can skip the row and continue.
std::optional<lob::Order> parse_order(const std::vector<std::string>& f) {
    if (f.size() < 7) {
        return std::nullopt;
    }
    const std::optional<lob::OrderType> type = parse_type(f[3]);
    if (!type) {
        return std::nullopt;
    }
    lob::Order order;
    try {
        order.id        = static_cast<lob::OrderId>(std::stoull(f[0]));
        order.symbol    = f[1];
        order.type      = *type;
        order.price     = static_cast<lob::Price>(std::stoll(f[4]));
        order.quantity  = static_cast<lob::Quantity>(std::stoull(f[5]));
        order.timestamp = static_cast<lob::Timestamp>(std::stoull(f[6]));
    } catch (const std::exception&) {
        return std::nullopt;
    }
    order.remaining_quantity = order.quantity;

    if (order.type == lob::OrderType::Cancel) {
        order.side = lob::Side::Buy;  // unused for cancels
    } else {
        const std::optional<lob::Side> side = parse_side(f[2]);
        if (!side) {
            return std::nullopt;
        }
        order.side = *side;
    }
    return order;
}

bool is_header(const std::vector<std::string>& fields) {
    return !fields.empty() && to_lower(fields[0]) == "order_id";
}

void write_trades(const std::string& path, const std::vector<lob::Trade>& trades) {
    std::ofstream out(path);
    if (!out) {
        std::cerr << "error: cannot open output file '" << path << "'\n";
        return;
    }
    out << "trade_id,buy_order_id,sell_order_id,symbol,execution_price,"
           "execution_quantity,timestamp\n";
    for (const lob::Trade& t : trades) {
        out << t.trade_id << ',' << t.buy_order_id << ',' << t.sell_order_id
            << ',' << t.symbol << ',' << t.execution_price << ','
            << t.execution_quantity << ',' << t.timestamp << '\n';
    }
}

void print_summary(const lob::MatchingEngine& engine,
                   std::size_t rows_read,
                   std::size_t parse_errors,
                   std::size_t trades_written,
                   double elapsed_seconds) {
    const lob::EngineStats& s = engine.stats();

    std::cout << "\n=== Simulation summary ===\n";
    std::cout << "rows read              : " << rows_read << '\n';
    std::cout << "parse errors (skipped) : " << parse_errors << '\n';
    std::cout << "messages processed     : " << s.orders_processed << '\n';
    std::cout << "orders accepted        : " << s.orders_accepted << '\n';
    std::cout << "orders rejected        : " << s.orders_rejected << '\n';
    std::cout << "cancels succeeded      : " << s.cancels_succeeded << '\n';
    std::cout << "trades executed        : " << s.trades_executed << '\n';
    std::cout << "total executed volume  : " << s.volume_executed << '\n';
    std::cout << "trades written         : " << trades_written << '\n';
    std::cout << "active resting orders  : " << engine.total_active_orders() << '\n';

    if (elapsed_seconds > 0.0) {
        const double ops =
            static_cast<double>(s.orders_processed) / elapsed_seconds;
        std::cout << "processing time (s)    : " << elapsed_seconds << '\n';
        std::cout << "throughput (msg/s)     : " << ops << '\n';
    }

    std::cout << "\n--- Final book state (per symbol) ---\n";
    for (const std::string& sym : engine.symbols()) {
        const lob::OrderBook* book = engine.book(sym);
        if (book == nullptr) {
            continue;
        }
        const std::optional<lob::Price> bid = book->best_bid();
        const std::optional<lob::Price> ask = book->best_ask();

        std::cout << sym << ": ";
        std::cout << "best_bid=" << (bid ? std::to_string(*bid) : "none");
        std::cout << " best_ask=" << (ask ? std::to_string(*ask) : "none");
        if (bid && ask) {
            std::cout << " spread=" << (*ask - *bid);
        } else {
            std::cout << " spread=n/a";
        }
        std::cout << " active_orders=" << book->active_order_count() << '\n';
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << (argc > 0 ? argv[0] : "lob_sim")
                  << " <input.csv> [output_trades.csv]\n";
        return 1;
    }

    const std::string input_path = argv[1];
    const std::string output_path = (argc >= 3) ? argv[2] : "trades.csv";

    std::ifstream in(input_path);
    if (!in) {
        std::cerr << "error: cannot open input file '" << input_path << "'\n";
        return 1;
    }

    // Load and parse the full order flow first so timing measures only matching.
    std::vector<lob::Order> orders;
    std::size_t rows_read = 0;
    std::size_t parse_errors = 0;
    std::string line;
    bool first_line = true;
    while (std::getline(in, line)) {
        if (trim(line).empty()) {
            continue;
        }
        const std::vector<std::string> fields = split_csv(line);
        if (first_line) {
            first_line = false;
            if (is_header(fields)) {
                continue;  // skip header row
            }
        }
        ++rows_read;
        std::optional<lob::Order> order = parse_order(fields);
        if (!order) {
            ++parse_errors;
            std::cerr << "warning: skipping malformed row: " << line << '\n';
            continue;
        }
        orders.push_back(std::move(*order));
    }

    lob::MatchingEngine engine;
    std::vector<lob::Trade> all_trades;

    const auto start = std::chrono::steady_clock::now();
    for (const lob::Order& order : orders) {
        lob::ProcessResult result = engine.process(order);
        for (lob::Trade& t : result.trades) {
            all_trades.push_back(std::move(t));
        }
    }
    const auto stop = std::chrono::steady_clock::now();
    const double elapsed =
        std::chrono::duration<double>(stop - start).count();

    write_trades(output_path, all_trades);
    print_summary(engine, rows_read, parse_errors, all_trades.size(), elapsed);
    std::cout << "\ntrades written to: " << output_path << '\n';
    return 0;
}
