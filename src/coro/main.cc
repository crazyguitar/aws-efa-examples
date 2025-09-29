#include <cstring>
#include <iostream>
#include <string>

#include "common/coro.h"
#include "common/runner.h"
#include "common/timer.h"

Coro<> Start() {
  std::cout << "Start" << std::endl;
  co_await Sleep(std::chrono::milliseconds(3000));
  std::cout << "Done" << std::endl;
  co_return;
}

int main(int argc, char *argv[]) { Run(Start()); }
