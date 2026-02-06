#include "lib.hpp"

#include "infra/infra.hpp"
#include "math/math.hpp"
#include "trader/trader.hpp"

#include <string>

namespace magus2 {

std::string hello() {
  const int sum = infra::ping() + math::ping() + trader::ping();
  return "magus2 hello: " + std::to_string(sum);
}

}  // namespace magus2
