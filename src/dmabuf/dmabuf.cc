#include <cstring>
#include <iostream>
#include <random>
#include <string>

#include "common/coro.h"
#include "common/efa.h"
#include "common/gpuloc.h"
#include "common/mpi.h"
#include "common/net.h"
#include "common/runner.h"
#include "common/timer.h"

#define MSGSIZE(msg) (sizeof(Message) + (sizeof(CUDARegion) * msg->num))

struct CUDARegion {
  uint64_t addr;
  uint64_t size;
  uint64_t key;
};

struct Message {
  int rank;
  size_t num;  // number of items
  uint64_t seed;

  CUDARegion &operator[](int i) {
    auto base = (CUDARegion *)((char *)this + sizeof(*this));
    return base[i];
  }
};

static void AllGatherAddr(const char *addr, int rank, std::string &endpoints) {
  std::memcpy(endpoints.data() + ENDPOINT_IDX(rank), addr, kMaxAddrSize);
  MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, endpoints.data(), kMaxAddrSize, MPI_BYTE, MPI_COMM_WORLD);
}

static Message *AllocMessage(Conn *conn) {
  ASSERT(!!conn);
  std::mt19937_64 rng(0x123456789UL);

  auto &mpi = MPI::Get();
  auto &buffer = conn->GetSendBuffer();
  auto data = (Message *)buffer.GetData();
  auto &cuda_buffer = conn->GetReadBuffer();
  auto cuda_data = cuda_buffer.GetData();
  auto cuda_mr = cuda_buffer.GetMR();
  auto cuda_key = cuda_mr->key;
  auto &header = *data;
  auto &payload = (*data)[0];

  header.rank = mpi.GetWorldRank();
  header.num = 1;
  header.seed = rng();
  payload.addr = (uint64_t)cuda_data;
  payload.size = kMemoryRegionSize;
  payload.key = cuda_key;
  return data;
}

static std::vector<uint8_t> RandBuffer(uint64_t seed, size_t size) {
  ASSERT(size % sizeof(uint64_t) == 0);
  std::vector<uint8_t> buf(size);
  std::mt19937_64 gen(seed);
  std::uniform_int_distribution<uint64_t> dist;
  for (size_t i = 0; i < size; i += sizeof(uint64_t)) *(uint64_t *)(buf.data() + i) = dist(gen);
  return buf;
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

static bool Verify(char *cuda_buffer, uint64_t seed, size_t size) {
  auto expected = RandBuffer(seed, size);
  auto actual = std::vector<uint8_t>(size, 0);
  CUDA_CHECK(cudaMemcpy(actual.data(), (uint8_t *)cuda_buffer, size, cudaMemcpyDeviceToHost));
  return expected == actual;
}

Coro<Message *> Handshake(Conn *conn, Message *req) {
  co_await conn->Send((char *)req, MSGSIZE(req));
  auto [buf, size] = co_await conn->Recv();
  auto resp = (Message *)buf;
  ASSERT(MSGSIZE(resp) == size);
  co_return resp;
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
  auto req = AllocMessage(conn);
  auto local_seed = req->seed;
  auto resp = co_await Handshake(conn, req);
  auto &region = (*resp)[0];
  uint64_t imm_data = 0x123;

  // clang-format off
  std::cout << "[RANK:" << rank << "]"
            << " dst_rank=" << resp->rank
            << " num=" << resp->num
            << " seed=" << resp->seed
            << " addr=" << region.addr
            << " size=" << region.size
            << " key=" << region.key << std::endl;
  // clang-format on

  constexpr size_t size = 8 << 20;  // 8 MB
  auto buf = RandBuffer(resp->seed, size);
  auto fut = Future(conn->Read(imm_data));
  co_await conn->Write((char *)buf.data(), buf.size(), region.addr, region.key, imm_data);
  auto cuda_buffer = co_await fut;
  ASSERT(Verify(cuda_buffer, local_seed, size));
}

int main(int argc, char *argv[]) { Run(Start()); }
