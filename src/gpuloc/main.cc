#include <iostream>

#include "common/gpuloc.h"

/**
 * @brief Main entry point
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit status code
 */
int main(int argc, char *argv[]) {
  auto &loc = GPUloc::Get();
  std::cout << loc;
}
