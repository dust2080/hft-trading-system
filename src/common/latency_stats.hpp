#pragma once

#include <vector>
#include <algorithm>
#include <numeric>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include <mutex>

namespace hft {

/**
 * Latency statistics collector.
 * Thread-safe for concurrent recording.
 */
class LatencyStats {
public:
    explicit LatencyStats(const std::string& name, size_t reserve_size = 100000)
        : name_(name) {
        samples_.reserve(reserve_size);
    }
    
    // Record a latency sample (in nanoseconds)
    void Record(int64_t latency_ns) {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.push_back(latency_ns);
    }
    
    // Get number of samples
    size_t Count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return samples_.size();
    }
    
    // Calculate and return statistics
    struct Stats {
        size_t count;
        double min_ns;
        double max_ns;
        double mean_ns;
        double median_ns;
        double p50_ns;
        double p90_ns;
        double p99_ns;
        double p999_ns;
    };
    
    Stats Calculate() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (samples_.empty()) {
            return Stats{0, 0, 0, 0, 0, 0, 0, 0, 0};
        }
        
        // Make a copy for sorting
        std::vector<int64_t> sorted = samples_;
        std::sort(sorted.begin(), sorted.end());
        
        size_t n = sorted.size();
        double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
        
        return Stats{
            .count = n,
            .min_ns = static_cast<double>(sorted.front()),
            .max_ns = static_cast<double>(sorted.back()),
            .mean_ns = sum / n,
            .median_ns = static_cast<double>(sorted[n / 2]),
            .p50_ns = static_cast<double>(sorted[n * 50 / 100]),
            .p90_ns = static_cast<double>(sorted[n * 90 / 100]),
            .p99_ns = static_cast<double>(sorted[n * 99 / 100]),
            .p999_ns = static_cast<double>(sorted[n * 999 / 1000])
        };
    }
    
    // Format statistics as string
    std::string ToString() const {
        auto s = Calculate();
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0);
        oss << name_ << " Latency Statistics:\n";
        oss << "  Count:  " << s.count << " samples\n";
        oss << "  Min:    " << s.min_ns << " ns\n";
        oss << "  Mean:   " << s.mean_ns << " ns\n";
        oss << "  Median: " << s.median_ns << " ns\n";
        oss << "  P90:    " << s.p90_ns << " ns\n";
        oss << "  P99:    " << s.p99_ns << " ns\n";
        oss << "  P99.9:  " << s.p999_ns << " ns\n";
        oss << "  Max:    " << s.max_ns << " ns\n";
        
        // Convert to microseconds for readability
        oss << "\n  In microseconds:\n";
        oss << std::setprecision(2);
        oss << "  Mean:   " << s.mean_ns / 1000.0 << " μs\n";
        oss << "  P99:    " << s.p99_ns / 1000.0 << " μs\n";
        oss << "  P99.9:  " << s.p999_ns / 1000.0 << " μs\n";
        
        return oss.str();
    }
    
    // Reset all samples
    void Reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.clear();
    }
    
    const std::string& Name() const { return name_; }

private:
    std::string name_;
    mutable std::mutex mutex_;
    std::vector<int64_t> samples_;
};

}  // namespace hft