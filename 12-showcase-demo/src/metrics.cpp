#include "showcase/metrics.hpp"

#include <fstream>
#include <optional>
#include <string>

namespace showcase {
namespace {

[[nodiscard]] bool read_loadavg(double& load_1m) {
    std::ifstream in("/proc/loadavg");
    if (!in) {
        return false;
    }
    in >> load_1m;
    return static_cast<bool>(in);
}

[[nodiscard]] bool read_cpu_times(std::uint64_t& idle, std::uint64_t& total) {
    std::ifstream in("/proc/stat");
    if (!in) {
        return false;
    }
    std::string cpu;
    std::uint64_t user = 0, nice = 0, system = 0, idle_v = 0, iowait = 0, irq = 0, softirq = 0,
                  steal = 0;
    in >> cpu >> user >> nice >> system >> idle_v >> iowait >> irq >> softirq >> steal;
    if (cpu != "cpu") {
        return false;
    }
    idle = idle_v + iowait;
    total = user + nice + system + idle + irq + softirq + steal;
    return true;
}

[[nodiscard]] std::optional<double> read_cpu_temp_c() {
    for (int i = 0; i < 8; ++i) {
        const std::string path =
            "/sys/class/thermal/thermal_zone" + std::to_string(i) + "/temp";
        std::ifstream in(path);
        long milli = 0;
        if (in >> milli) {
            if (milli > 1000) {
                return static_cast<double>(milli) / 1000.0;
            }
            return static_cast<double>(milli);
        }
    }
    return std::nullopt;
}

}  // namespace

Sample MetricsSampler::take(std::size_t queue_depth, std::uint64_t drops) {
    Sample s;
    s.queue_depth = queue_depth;
    s.drops = drops;

    (void)read_loadavg(s.load_1m);
    s.cpu_temp_c = read_cpu_temp_c();

    std::uint64_t idle = 0;
    std::uint64_t total = 0;
    if (read_cpu_times(idle, total)) {
        if (have_prev_cpu_ && total > prev_total_) {
            const auto didle = idle - prev_idle_;
            const auto dtotal = total - prev_total_;
            if (dtotal > 0) {
                s.cpu_percent =
                    100.0 * (1.0 - static_cast<double>(didle) / static_cast<double>(dtotal));
                if (s.cpu_percent < 0) {
                    s.cpu_percent = 0;
                }
                if (s.cpu_percent > 100) {
                    s.cpu_percent = 100;
                }
            }
        }
        prev_idle_ = idle;
        prev_total_ = total;
        have_prev_cpu_ = true;
    }

    return s;
}

}  // namespace showcase
