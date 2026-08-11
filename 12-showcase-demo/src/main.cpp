#include "showcase/demo.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    showcase::DemoConfig config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--seconds" && i + 1 < argc) {
            config.run_seconds = std::atoi(argv[++i]);
        } else if (arg == "--samples" && i + 1 < argc) {
            config.report_every_samples = std::atoi(argv[++i]);
        } else if (arg == "--period-ms" && i + 1 < argc) {
            config.sample_period_ms = std::atoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout
                << "Usage: showcase_demo [--seconds N] [--samples N] [--period-ms N]\n"
                << "  --seconds    run this many seconds then quit (default 2; 0 = forever)\n"
                << "  --samples    aggregate every N samples (default 5)\n"
                << "  --period-ms  sample period (default 200)\n";
            return 0;
        }
    }
    if (config.run_seconds < 0 || config.report_every_samples <= 0 ||
        config.sample_period_ms <= 0) {
        std::cerr << "invalid config\n";
        return 2;
    }
    return showcase::run_demo(config);
}
