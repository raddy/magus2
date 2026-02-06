#include "lib.hpp"

#include <string>

int main() {
  const std::string msg = magus2::hello();
  return msg.rfind("magus2 hello:", 0) == 0 ? 0 : 1;
}
