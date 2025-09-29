#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "common/coro.h"
#include "common/efa.h"
#include "common/gpuloc.h"
#include "common/mpi.h"
#include "common/net.h"
#include "common/progress.h"
#include "common/runner.h"
#include "common/taskset.h"
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

class Peer : private NoCopy {
 public:
  Peer() = delete;
  Peer(int peer) : net_{Net()}, peer_{peer} {
    auto &mpi = MPI::Get();
    auto rank = mpi.GetWorldRank();
    auto &loc = GPUloc::Get();
    auto local_rank = mpi.GetLocalRank();
    auto &affinity = loc.GetGPUAffinity()[local_rank];
    auto cpu = affinity.cores[local_rank]->logical_index;
    auto efa = affinity.efas[local_rank].second;

    std::cout << "[RANK:" << rank << "] GPU(" << local_rank << ") CPU(" << cpu << ")" << std::endl;
    cudaSetDevice(local_rank);
    Taskset::Set(cpu);
    net_.Open(efa);
    conn_ = Connect(net_, peer);
    ASSERT(!!conn_);
  }

 private:
  inline static Conn *Connect(Net &net, int peer) {
    auto &mpi = MPI::Get();
    int rank = mpi.GetWorldRank();
    char remote[kMaxAddrSize] = {0};
    std::string endpoints(mpi.GetWorldSize() * kMaxAddrSize, 0);
    AllGatherAddr(net.GetAddr(), rank, endpoints);
    std::memcpy(remote, endpoints.data() + ENDPOINT_IDX(peer), kMaxAddrSize);
    return net.Connect(remote);
  }

 protected:
  Net net_;
  int peer_;
  Conn *conn_;
};

class Writer : public Peer {
 public:
  Writer() = delete;
  Writer(int peer) : Peer(peer) {}

  Coro<> Handshake() {
    auto [buf, size] = co_await conn_->Recv();
    auto resp = (Message *)buf;
    ASSERT(MSGSIZE(resp) == size);
    ASSERT(resp->rank == peer_);
    peer_seed_ = resp->seed;

    peer_regions_.resize(resp->num);
    for (size_t i = 0; i < resp->num; ++i) {
      peer_regions_[i] = (*resp)[0];
    }
  }

 private:
  uint64_t peer_seed_;
  std::vector<CUDARegion> peer_regions_;
};

class Reader : public Peer {
 public:
  Reader() = delete;
  Reader(int peer) : Peer(peer) {}

  Coro<> Handshake() {
    auto req = Alloc(conn_);
    auto size = co_await conn_->Send((char *)req, MSGSIZE(req));
    ASSERT(size == MSGSIZE(req));
  }

 private:
  inline static Message *Alloc(Conn *conn) {
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
};

Coro<> StartWriter(size_t page_size, size_t num_pages) {
  auto &mpi = MPI::Get();
  ASSERT(mpi.GetWorldRank() == 0);
  auto peer = 1;
  auto writer = Writer(peer);
  co_await writer.Handshake();
}

Coro<> StartReader(size_t page_size, size_t num_pages) {
  auto &mpi = MPI::Get();
  ASSERT(mpi.GetWorldRank() == 1);
  auto peer = 0;
  auto reader = Reader(peer);
  co_await reader.Handshake();
}

int main(int argc, char *argv[]) {
  auto &mpi = MPI::Get();
  // assumption: 2 nodes and nproc per ndoe = 1
  ASSERT(mpi.GetWorldSize() == 2);
  ASSERT(mpi.GetLocalSize() == 1);

  constexpr size_t page_size = 128 * 8 * 2 * 16 * sizeof(uint16_t);  // 64k
  constexpr size_t num_pages = 1000;
  if (mpi.GetWorldRank() == 0) {
    Run(StartWriter(page_size, num_pages));
  } else {
    Run(StartReader(page_size, num_pages));
  }
}
