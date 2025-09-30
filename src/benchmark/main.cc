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
constexpr uint32_t kImmData = 0x123;

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
  Peer(int peer, size_t page_size, size_t num_pages)
      : net_{Net()}, peer_{peer}, page_size_{page_size}, num_pages_{num_pages}, size_{page_size * num_pages} {
    auto &mpi = MPI::Get();
    auto rank = mpi.GetWorldRank();
    auto &loc = GPUloc::Get();
    auto local_rank = mpi.GetLocalRank();
    auto &affinity = loc.GetGPUAffinity()[local_rank];
    auto cpu = affinity.cores[local_rank]->logical_index;
    auto efa = affinity.efas[local_rank].second;
    total_bw_ = efa->nic->link_attr->speed;

    std::cout << "[RANK:" << rank << "] GPU(" << local_rank << ") CPU(" << cpu << ")" << std::endl;
    cudaSetDevice(local_rank);
    Taskset::Set(cpu);
    net_.Open(efa);
    conn_ = Connect(net_, peer);
    ASSERT(!!conn_);
    auto total = page_size_ * num_pages_;
    std::cout << fmt::format("page_size={} num_pages={} total={} mem_size={}", page_size_, num_pages_, total, kMemoryRegionSize) << std::endl;
    ASSERT((page_size_ * num_pages_) <= conn_->GetReadBuffer().GetSize());
    ASSERT((page_size_ * num_pages_) <= conn_->GetWriteBuffer().GetSize());
  }

 protected:
  inline static Conn *Connect(Net &net, int peer) {
    auto &mpi = MPI::Get();
    int rank = mpi.GetWorldRank();
    char remote[kMaxAddrSize] = {0};
    std::string endpoints(mpi.GetWorldSize() * kMaxAddrSize, 0);
    AllGatherAddr(net.GetAddr(), rank, endpoints);
    std::memcpy(remote, endpoints.data() + ENDPOINT_IDX(peer), kMaxAddrSize);
    return net.Connect(remote);
  }

  inline static std::vector<uint8_t> RandBuffer(uint64_t seed, size_t size) {
    ASSERT(size % sizeof(uint64_t) == 0);
    std::vector<uint8_t> buf(size);
    std::mt19937_64 gen(seed);
    std::uniform_int_distribution<uint64_t> dist;
    for (size_t i = 0; i < size; i += sizeof(uint64_t)) *(uint64_t *)(buf.data() + i) = dist(gen);
    return buf;
  }

  inline static bool Verify(char *cuda_buffer, uint64_t seed, size_t size) {
    auto expected = RandBuffer(seed, size);
    auto actual = std::vector<uint8_t>(size, 0);
    CUDA_CHECK(cudaMemcpy(actual.data(), (uint8_t *)cuda_buffer, size, cudaMemcpyDeviceToHost));
    return expected == actual;
  }

 protected:
  Net net_;
  int peer_;
  Conn *conn_;
  size_t page_size_;
  size_t num_pages_;
  size_t size_;
  size_t total_bw_;
  std::mt19937_64 rng_{0x123456789UL};
};

class Writer : public Peer {
 public:
  Writer() = delete;
  Writer(int peer, size_t page_size, size_t num_pages) : Peer(peer, page_size, num_pages) {
    auto buffer = RandBuffer(peer_seed_, size_);
    auto cuda_buffer = (char *)conn_->GetWriteBuffer().GetData();
    CUDA_CHECK(cudaMemcpy(cuda_buffer, buffer.data(), size_, cudaMemcpyHostToDevice));
  }

  Coro<> Handshake() {
    auto [buf, size] = co_await conn_->Recv();
    auto resp = (Message *)buf;
    ASSERT(MSGSIZE(resp) == size);
    ASSERT(resp->rank == peer_);
    peer_seed_ = resp->seed;
    peer_regions_.resize(resp->num);
    for (size_t i = 0; i < resp->num; ++i) {
      peer_regions_[i] = (*resp)[i];
    }
  }

  Coro<> Write(size_t repeat) {
    auto total_ops = repeat * peer_regions_.size() * num_pages_;
    auto progress = Progress(total_ops, total_bw_);
    size_t ops = 0;
    for (size_t i = 0; i < repeat; ++i) {
      co_await WriteOne(progress, ops);
    }
  }

  Coro<> WriteOne(Progress &progress, size_t &ops) {
    auto cuda_buffer = conn_->GetWriteBuffer().GetData();
    for (auto &region : peer_regions_) {
      for (size_t i = 0; i < num_pages_; ++i) {
        auto base = (char *)cuda_buffer + i * page_size_;
        auto addr = region.addr + i * page_size_;
        auto key = region.key;
        auto is_final = (i == num_pages_ - 1);
        auto imm_data = is_final ? kImmData : 0;
        co_await conn_->Write(base, page_size_, addr, key, imm_data);
        ++ops;
        auto now = std::chrono::high_resolution_clock::now();
        progress.Print(now, page_size_, ops);
      }
    }
  }

 private:
  uint64_t peer_seed_;
  std::vector<CUDARegion> peer_regions_;
};

class Reader : public Peer {
 public:
  Reader() = delete;
  Reader(int peer, size_t page_size, size_t num_pages) : Peer(peer, page_size, num_pages) {}

  Coro<> Handshake() {
    auto req = Alloc(conn_);
    auto size = co_await conn_->Send((char *)req, MSGSIZE(req));
    ASSERT(size == MSGSIZE(req));
  }

  Coro<> Read(size_t repeat) {
    for (size_t i = 0; i < repeat; ++i) {
      co_await conn_->Read(kImmData);
    }
  }

  Coro<> ReadOne() { co_await conn_->Read(kImmData); }

 private:
  inline Message *Alloc(Conn *conn) {
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
    header.seed = rng_();
    payload.addr = (uint64_t)cuda_data;
    payload.size = kMemoryRegionSize;
    payload.key = cuda_key;
    return data;
  }
};

Coro<> StartWriter(size_t page_size, size_t num_pages, size_t repeat) {
  auto &mpi = MPI::Get();
  ASSERT(mpi.GetWorldRank() == 0);
  auto peer = 1;
  auto writer = Writer(peer, page_size, num_pages);
  co_await writer.Handshake();
  co_await writer.Write(repeat);
}

Coro<> StartReader(size_t page_size, size_t num_pages, size_t repeat) {
  auto &mpi = MPI::Get();
  ASSERT(mpi.GetWorldRank() == 1);
  auto peer = 0;
  auto reader = Reader(peer, page_size, num_pages);
  co_await reader.Handshake();
  co_await reader.Read(repeat);
}

int main(int argc, char *argv[]) {
  auto &mpi = MPI::Get();
  // assumption: 2 nodes and nproc per ndoe = 1
  ASSERT(mpi.GetWorldSize() == 2);
  ASSERT(mpi.GetLocalSize() == 1);

  constexpr size_t page_size = 128 * 8 * 2 * 16 * sizeof(uint16_t);
  constexpr size_t num_pages = 1000;
  constexpr size_t repeat = 500;
  if (mpi.GetWorldRank() == 0) {
    Run(StartWriter(page_size, num_pages, repeat));
  } else {
    Run(StartReader(page_size, num_pages, repeat));
  }
}
