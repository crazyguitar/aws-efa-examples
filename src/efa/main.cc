#include <cstring>
#include <iostream>
#include <string>

#include "common/buffer.h"
#include "common/coro.h"
#include "common/efa.h"
#include "common/mpi.h"
#include "common/net.h"
#include "common/runner.h"
#include "common/timer.h"

void AllGatherAddr(const char *addr, int rank, std::string &endpoints) {
  std::memcpy(endpoints.data() + ENDPOINT_IDX(rank), addr, kMaxAddrSize);
  MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, endpoints.data(), kMaxAddrSize, MPI_BYTE, MPI_COMM_WORLD);
}

Coro<> Start() {
  auto &mpi = MPI::Get();
  auto &efa = EFA::Get();
  auto net = Net();
  auto rank = mpi.GetWorldRank();
  auto world_size = mpi.GetWorldSize();
  char remote[kMaxAddrSize] = {0};
  std::string endpoints(mpi.GetWorldSize() * kMaxAddrSize, 0);

  net.Open(efa.GetEFAInfo());
  AllGatherAddr(net.GetAddr(), rank, endpoints);
  std::memcpy(remote, endpoints.data() + ENDPOINT_IDX((rank + 1) % world_size), kMaxAddrSize);

  auto conn = net.Connect(remote);
  auto src = rank;
  auto dst = (rank + 1) % world_size;
  std::string data;

  Buffer rbuf{kBufferSize};
  Buffer sbuf{kBufferSize};
  rbuf.Register(conn->GetDomain());
  sbuf.Register(conn->GetDomain());

  // ping/pong
  std::string msg = fmt::format("[rank:{}] [{}] -> [{}]", rank, src, dst);
  std::memcpy(sbuf.GetData(), msg.data(), msg.size());
  std::cout << "send data:" << msg << std::endl;
  co_await conn->Send(sbuf, msg.size());
  auto len = co_await conn->Recv(rbuf);
  auto buf = (char *)rbuf.GetData();
  for (size_t i = 0; i < len; ++i) data += buf[i];
  std::cout << "recv data: " << data << std::endl;
}

int main(int argc, char *argv[]) { Run(Start()); }
