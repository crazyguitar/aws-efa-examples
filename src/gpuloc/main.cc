#include <iostream>

#include "common/gpuloc.h"

int main(int argc, char *argv[]) {
  auto &loc = GPUloc::Get();
  std::cout << loc;
}
