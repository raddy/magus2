#include <mvp/logging.hpp>
#include <mvp/runtime.hpp>

#include <chrono>
#include <iostream>

int main() {
  using namespace magus2::mvp;

  logging::start();

  MvpConfig config;
  config.ingress_depth = 64;
  config.tick_depth = 64;
  config.order_depth = 32;
  config.ack_depth = 32;
  config.tick_interval_us = 50;
  config.order_every_n_ticks = 8;

  const RunResult result = run_for(std::chrono::milliseconds(500), config);

  if (!result.built) {
    std::cerr << "mvp topology build failed: " << result.error << '\n';
    logging::stop();
    return 1;
  }
  if (!result.started) {
    std::cerr << "mvp runtime start failed: " << result.error << '\n';
    logging::stop();
    return 1;
  }

  std::cout << "mvp stats"
            << " md_sent=" << result.stats.md_ticks_sent
            << " strat_seen=" << result.stats.strat_ticks_seen
            << " orders_sent=" << result.stats.strat_orders_sent
            << " or_seen=" << result.stats.or_orders_seen
            << " or_acks=" << result.stats.or_acks_sent
            << " strat_acks=" << result.stats.strat_acks_seen
            << " tick_ow_avg_ns=" << result.stats.tick_one_way_avg_ns()
            << " tick_ow_max_ns=" << result.stats.tick_one_way_max_ns
            << " order_rtt_avg_ns=" << result.stats.order_rtt_avg_ns()
            << " order_rtt_max_ns=" << result.stats.order_rtt_max_ns
            << " trace_ticks=" << result.stats.trace_ticks_seen
            << " trace_acks=" << result.stats.trace_acks_seen
            << '\n';

  if (!flow_looks_valid(result.stats)) {
    std::cerr << "mvp flow validation failed\n";
    logging::stop();
    return 2;
  }

  logging::stop();
  return 0;
}
