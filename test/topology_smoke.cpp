#include <mvp/harness.hpp>

#include <chrono>

int main() {
  using namespace magus2::mvp;
  const RunResult result = run_for(std::chrono::milliseconds(300));
  if (!result.built || !result.started) {
    return 1;
  }

  return flow_looks_valid(result.stats) ? 0 : 2;
}
