#include <cstring>
#include <iostream>
#include <string>

#include "common/coro.h"
#include "common/efa.h"
#include "common/gpuloc.h"
#include "common/mpi.h"
#include "common/net.h"
#include "common/runner.h"
#include "common/timer.h"

#define MESSAGE_SIZE(msg) (sizeof(Message) + (msg->header.size * msg->header.num))

struct Header {
  int rank;
  size_t size;  // size of each item
  size_t num;   // number of items
};

struct Message {
  Header header;
  void *payload;
};

struct CUDARegion {
  uint64_t addr;
  uint64_t size;
  uint64_t key;
};

static void AllGatherAddr(const char *addr, int rank, std::string &endpoints) {
  std::memcpy(endpoints.data() + ENDPOINT_IDX(rank), addr, kMaxAddrSize);
  MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, endpoints.data(), kMaxAddrSize, MPI_BYTE, MPI_COMM_WORLD);
}

static Message *AllocMessage(Conn *conn) {
  ASSERT(!!conn);
  auto &mpi = MPI::Get();
  auto &buffer = conn->GetSendBuffer();
  auto data = reinterpret_cast<Message *>(buffer.GetData());
  auto &cuda_buffer = conn->GetReadBuffer();
  auto cuda_data = cuda_buffer.GetData();
  auto cuda_mr = cuda_buffer.GetMR();
  auto cuda_key = cuda_mr->key;
  auto payload = reinterpret_cast<CUDARegion *>(data->payload);

  data->header.rank = mpi.GetWorldRank();
  data->header.size = sizeof(CUDARegion);
  data->header.num = 1;
  payload->addr = (uint64_t)cuda_data;
  payload->size = kMemoryRegionSize;
  payload->key = cuda_key;
  return data;
}

static Conn *Connect(Net &net, int dst) {
  auto &mpi = MPI::Get();
  auto rank = mpi.GetWorldRank();
  char remote[kMaxAddrSize] = {0};
  std::string endpoints(mpi.GetWorldSize() * kMaxAddrSize, 0);

  // all_gather peers' address
  AllGatherAddr(net.GetAddr(), rank, endpoints);
  std::memcpy(remote, endpoints.data() + ENDPOINT_IDX(dst), kMaxAddrSize);
  return net.Connect(remote);
}

Coro<> Handshake(Conn *conn) {
  auto send_msg = AllocMessage(conn);
  co_await conn->Send((char *)send_msg, MESSAGE_SIZE(send_msg));
  auto [buf, size] = co_await conn->Recv();
  auto recv_msg = (Message *)buf;
  ASSERT(MESSAGE_SIZE(recv_msg) == size);

  // dump recv info
  auto &header = recv_msg->header;
  std::cout << "[RANK:" << header.rank << "]" << " size=" << header.size << " num=" << header.num << std::endl;
}

Coro<> Start() {
  auto &mpi = MPI::Get();
  auto &efa = EFA::Get();
  auto net = Net();
  auto rank = mpi.GetWorldRank();
  auto world_size = mpi.GetWorldSize();
  auto dst = (rank + 1) % world_size;

  net.Open(efa.GetEFAInfo());
  auto conn = Connect(net, dst);
  co_await Handshake(conn);
}

int main(int argc, char *argv[]) { Run(Start()); }
