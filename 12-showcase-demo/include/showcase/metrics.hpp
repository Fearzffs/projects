#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace showcase {

/// One telemetry sample.
struct Sample {
    double load_1m{0};
    double cpu_percent{0};
    std::optional<double> cpu_temp_c;
    std::size_t queue_depth{0};
    std::uint64_t drops{0};
};

struct WindowStats {
    int window_index{0};
    int samples{0};
    double avg_cpu_percent{0};
    double avg_load_1m{0};
    std::optional<double> last_temp_c;
};

/// Reads Linux /proc (and thermal sysfs). Missing fields stay empty/zero.
class MetricsSampler {
public:
    Sample take(std::size_t queue_depth, std::uint64_t drops);

private:
    bool have_prev_cpu_{false};
    std::uint64_t prev_idle_{0};
    std::uint64_t prev_total_{0};
};

}  // namespace showcase
