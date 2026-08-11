#pragma once

namespace showcase {

struct DemoConfig {
    /// How long to stay in Running (seconds). 0 = run until killed.
    int run_seconds{2};
    /// Host sample period.
    int sample_period_ms{200};
    /// Aggregate + print every N samples (still stays in Running).
    int report_every_samples{5};
};

/// Run the telemetry showcase. Returns 0 on success.
[[nodiscard]] int run_demo(const DemoConfig& config = {});

}  // namespace showcase
