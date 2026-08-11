#include "showcase/demo.hpp"

#include <gtest/gtest.h>

TEST(ShowcaseDemo, RunsThenShutsDown) {
    showcase::DemoConfig config;
    config.run_seconds = 2;
    config.report_every_samples = 3;
    config.sample_period_ms = 50;
    EXPECT_EQ(showcase::run_demo(config), 0);
}
