#include <cstring>
#include <iostream>
#include <string>

#include "common/coro.h"
#include "common/efa.h"
#include "common/io.h"
#include "common/mpi.h"
#include "common/net.h"
#include "common/timer.h"
#include "common/utils.h"

void AllGatherAddr(const char *addr, int rank, std::string &endpoints) {
  std::memcpy(endpoints.data() + ENDPOINT_IDX(rank), addr, kMaxAddrSize);
  MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, endpoints.data(), kMaxAddrSize, MPI_BYTE, MPI_COMM_WORLD);
}

coro::Coro<> Start(IO &io) {
  std::cout << "Start" << std::endl;
  co_await Timer::Sleep(std::chrono::milliseconds(1000), io);
  std::cout << "Done" << std::endl;
  io.Stop();
}

int main(int argc, char *argv[]) {
  auto &mpi = MPI::Get();
  auto &efa = EFA::Get();
  auto &io = IO::Get();
  auto net = Net();
  auto rank = mpi.GetWorldRank();
  auto world_size = mpi.GetWorldSize();
  char remote[kMaxAddrSize] = {0};
  std::string endpoints(mpi.GetWorldSize() * kMaxAddrSize, 0);

  net.Open(efa.GetEFAInfo());
  AllGatherAddr(net.GetAddr(), rank, endpoints);
  std::memcpy(remote, endpoints.data() + ENDPOINT_IDX((rank + 1) % world_size), kMaxAddrSize);
  net.Connect(remote);
  io.Spawn(Start(io));
  io.Run();
}
