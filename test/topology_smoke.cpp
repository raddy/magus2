#include <mvp/runtime.hpp>

#include <chrono>

int main() {
  using namespace magus2::mvp;

  MvpConfig config;
  config.tick_depth = 64;
  config.order_depth = 32;
  config.ack_depth = 32;
  config.md_tick_interval_us = 50;
  config.order_every_n_ticks = 8;

  const RunResult result = run_for(std::chrono::milliseconds(300), config);
  if (!result.built || !result.started) {
    return 1;
  }

  return flow_looks_valid(result.stats) ? 0 : 2;
}
