#include <cstring>
#include <string>

#include "common/efa.h"
#include "common/mpi.h"
#include "common/net.h"
#include "common/utils.h"

void AllGatherAddr(const char *addr, int rank, std::string &endpoints) {
  std::memcpy(endpoints.data() + ENDPOINT_IDX(rank), addr, kMaxAddrSize);
  MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, endpoints.data(), kMaxAddrSize, MPI_BYTE, MPI_COMM_WORLD);
}

int main(int argc, char *argv[]) {
  auto &mpi = MPI::Get();
  auto &efa = EFA::Get();
  auto net = Net();
  auto rank = mpi.GetWorldRank();
  std::string endpoints(mpi.GetWorldSize() * kMaxAddrSize, 0);

  net.Open(efa.GetEFAInfo());
  AllGatherAddr(net.GetAddr(), rank, endpoints);
}
