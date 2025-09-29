#include "common/gpuloc.h"

#include <iostream>

int main(int argc, char *argv[]) {
  auto &loc = GPUloc::Get();
  std::cout << loc;
}
