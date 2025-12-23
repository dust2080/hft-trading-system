#include "order_book.hpp"
#include <iostream>
#include <random>
#include <vector>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <iomanip>

using namespace hft;

// Measure latency for a single operation
template<typename Func>
int64_t MeasureNanos(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

struct BenchmarkResult {
    std::string name;
    double min_ns;
    double max_ns;
    double mean_ns;
    double median_ns;
    double p99_ns;
    size_t iterations;
};

void PrintResult(const BenchmarkResult& r) {
    std::cout << std::left << std::setw(25) << r.name
              << " | min: " << std::setw(8) << std::fixed << std::setprecision(0) << r.min_ns
              << " | mean: " << std::setw(8) << r.mean_ns
              << " | median: " << std::setw(8) << r.median_ns
              << " | p99: " << std::setw(8) << r.p99_ns
              << " | max: " << std::setw(8) << r.max_ns
              << " ns (" << r.iterations << " iterations)\n";
}

BenchmarkResult AnalyzeLatencies(const std::string& name, std::vector<int64_t>& latencies) {
    std::sort(latencies.begin(), latencies.end());
    
    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    size_t n = latencies.size();
    
    return BenchmarkResult{
        .name = name,
        .min_ns = static_cast<double>(latencies.front()),
        .max_ns = static_cast<double>(latencies.back()),
        .mean_ns = sum / n,
        .median_ns = static_cast<double>(latencies[n / 2]),
        .p99_ns = static_cast<double>(latencies[static_cast<size_t>(n * 0.99)]),
        .iterations = n
    };
}

int main() {
    std::cout << "=== Order Book Benchmark ===\n\n";

    constexpr size_t kWarmupIterations = 10000;
    constexpr size_t kBenchmarkIterations = 100000;
    constexpr Price kBasePrice = 3000000;  // 30000.00
    constexpr Price kPriceRange = 10000;   // +/- 100.00

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<Price> price_dist(kBasePrice - kPriceRange, 
                                                     kBasePrice + kPriceRange);
    std::uniform_int_distribution<Quantity> qty_dist(1, 1000000000);  // 0.01 to 10.0 BTC
    std::uniform_int_distribution<int> side_dist(0, 1);

    OrderBook book("BTCUSDT", 2, 8);

    // Warmup: populate order book with initial levels
    std::cout << "Warming up (" << kWarmupIterations << " operations)...\n";
    for (size_t i = 0; i < kWarmupIterations; ++i) {
        Side side = static_cast<Side>(side_dist(gen));
        Price price = price_dist(gen);
        Quantity qty = qty_dist(gen);
        book.Update(side, price, qty);
    }
    std::cout << "Order book has " << book.GetLevelCount(Side::kBuy) << " bid levels, "
              << book.GetLevelCount(Side::kSell) << " ask levels\n\n";

    // Benchmark: Update operation
    std::cout << "Benchmarking Update() (" << kBenchmarkIterations << " operations)...\n";
    std::vector<int64_t> update_latencies;
    update_latencies.reserve(kBenchmarkIterations);

    for (size_t i = 0; i < kBenchmarkIterations; ++i) {
        Side side = static_cast<Side>(side_dist(gen));
        Price price = price_dist(gen);
        Quantity qty = qty_dist(gen);
        
        int64_t ns = MeasureNanos([&]() {
            book.Update(side, price, qty);
        });
        update_latencies.push_back(ns);
    }

    PrintResult(AnalyzeLatencies("Update()", update_latencies));

    // Benchmark: GetBestBid
    std::cout << "Benchmarking GetBestBid() (" << kBenchmarkIterations << " operations)...\n";
    std::vector<int64_t> best_bid_latencies;
    best_bid_latencies.reserve(kBenchmarkIterations);

    for (size_t i = 0; i < kBenchmarkIterations; ++i) {
        // Invalidate cache periodically
        if (i % 100 == 0) {
            Side side = static_cast<Side>(side_dist(gen));
            book.Update(side, price_dist(gen), qty_dist(gen));
        }

        int64_t ns = MeasureNanos([&]() {
            volatile auto result = book.GetBestBid();
            (void)result;
        });
        best_bid_latencies.push_back(ns);
    }

    PrintResult(AnalyzeLatencies("GetBestBid()", best_bid_latencies));

    // Benchmark: GetBestBid (cached)
    std::cout << "Benchmarking GetBestBid() cached (" << kBenchmarkIterations << " operations)...\n";
    std::vector<int64_t> cached_latencies;
    cached_latencies.reserve(kBenchmarkIterations);

    // Prime the cache
    book.GetBestBid();

    for (size_t i = 0; i < kBenchmarkIterations; ++i) {
        int64_t ns = MeasureNanos([&]() {
            volatile auto result = book.GetBestBid();
            (void)result;
        });
        cached_latencies.push_back(ns);
    }

    PrintResult(AnalyzeLatencies("GetBestBid() [cached]", cached_latencies));

    // Benchmark: GetTopLevels
    std::cout << "Benchmarking GetTopLevels(5) (" << kBenchmarkIterations / 10 << " operations)...\n";
    std::vector<int64_t> top_levels_latencies;
    top_levels_latencies.reserve(kBenchmarkIterations / 10);

    for (size_t i = 0; i < kBenchmarkIterations / 10; ++i) {
        Side side = static_cast<Side>(side_dist(gen));
        
        int64_t ns = MeasureNanos([&]() {
            volatile auto result = book.GetTopLevels(side, 5);
            (void)result;
        });
        top_levels_latencies.push_back(ns);
    }

    PrintResult(AnalyzeLatencies("GetTopLevels(5)", top_levels_latencies));

    // Benchmark: GetQuantityAt
    std::cout << "Benchmarking GetQuantityAt() (" << kBenchmarkIterations << " operations)...\n";
    std::vector<int64_t> qty_at_latencies;
    qty_at_latencies.reserve(kBenchmarkIterations);

    for (size_t i = 0; i < kBenchmarkIterations; ++i) {
        Side side = static_cast<Side>(side_dist(gen));
        Price price = price_dist(gen);
        
        int64_t ns = MeasureNanos([&]() {
            volatile auto result = book.GetQuantityAt(side, price);
            (void)result;
        });
        qty_at_latencies.push_back(ns);
    }

    PrintResult(AnalyzeLatencies("GetQuantityAt()", qty_at_latencies));

    // Summary
    std::cout << "\n=== Summary ===\n";
    std::cout << "Total updates processed: " << book.GetUpdateCount() << "\n";
    std::cout << "Final book size: " << book.GetLevelCount(Side::kBuy) << " bids, "
              << book.GetLevelCount(Side::kSell) << " asks\n";

    auto spread = book.GetSpread();
    if (spread) {
        std::cout << "Current spread: " << SymbolConfig::FixedToString(*spread, 2) << "\n";
    }

    return 0;
}