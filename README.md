# AWS EFA Examples

Step-by-step examples demonstrating high-performance RDMA data transfers using [libfabric](https://github.com/ofiwg/libfabric) with AWS Elastic Fabric Adapter (EFA).

## Features

- Host memory to host memory data transfers
- GPU memory transfers using DMA buffers
- C++20 coroutine-based implementation for improved code readability

## Why Coroutines?

Traditional event-driven APIs require callback functions that can make code
difficult to read and maintain. Managing global memory state across callbacks
is error-prone and can lead to invalid memory access. This project uses C++20
coroutines to provide a cleaner, more maintainable programming model.

## Examples

* **[info](src/info)** - Query EFA device information and capabilities using libfabric APIs
* **[coro](src/coro)** - Basic coroutine event loop implementation for asynchronous programming
* **[efa](src/efa)** - Host-to-host memory transfers using coroutines and EFA RDMA
* **[dmabuf](src/dmabuf)** - GPU memory transfers using DMA buffers for zero-copy operations
* **[gpuloc](src/gpuloc)** - Discover GPU topology and NUMA affinity using [hwloc](https://github.com/open-mpi/hwloc)
* **[affinity](src/affinity)** - GPU memory transfers optimized with CPU-GPU affinity
* **[batch](src/batch)** - High-throughput batch operations with affinity optimization

## Development

```bash
# build an enroot sqush file
make sqush

# launch an interactive enroot environment
enroot create --name efa efa+latest.sqsh
enroot start --mount /fsx:/fsx efa /bin/bash

# build examples
make build
```

## Summary

### Coroutine

The [coro](src/coro) example demonstrates event-driven programming using C++20 coroutines.
To understand this implementation, you can start with the [event loop](src/coro/include/coro.h)
to see how coroutines are scheduled.

You can observe that the event loop has three key components:
- **Selector**: Queries I/O readiness (read/write events)
- **Schedule queue**: Holds coroutines waiting for events
- **Ready queue**: Contains coroutines ready for execution

Two main functions drive the loop:
- **Select()**: Uses the selector to check if events are ready. For EFA, this queries the [completion queue](https://github.com/ofiwg/libfabric/blob/7d60749967a425fff75dd36df03757ab871b0b4e/man/fi_cq.3.md) for fabric operation completion
- **RunOne()**: Checks if scheduled coroutines are ready, moving them to the ready queue for execution

![alt Event Loop](imgs/io.png)

### Fabric Initialization

#### Device Discovery

The [info](src/info) example demonstrates device discovery using `fi_getinfo()` to
query available EFA devices. This API is essential for selecting the appropriate
EFA device for communication.

Query EFA devices using the command line tool:
```bash
fi_info -p efa
```

Example output:
```
provider: efa
    fabric: efa
    domain: rdmap201s0-dgrm
    version: 201.0
    type: FI_EP_DGRAM
    protocol: FI_PROTO_EFA
```

#### Fabric Object Hierarchy

The [efa](src/efa) example shows ping/pong communication between two nodes.
Before communication begins, several fabric objects must be initialized in a
specific hierarchy:

**Components:**
- **Fabric**: Collection of hardware devices and resources
- **Domain**: Represents a single network interface (similar to NIC)
- **Endpoint**: Communication channel for sending/receiving data
- **Address Vector (AV)**: Stores remote peer addresses for communication
- **Completion Queue (CQ)**: Event notification mechanism for completed operations
- **Memory Region (MR)**: Registered memory accessible by EFA hardware for RDMA

Note that the Completion Queue functions like `select()` or `epoll()` in
traditional network programming, allowing applications to poll for completion
events on SEND/RECV/WRITE operations.

![alt Fabric](imgs/fabric.png)

#### Memory Region Registration

Memory Regions enable zero-copy RDMA operations by allowing EFA hardware direct
access to application memory. To register a memory region, you must provide:

- **Memory address**: Pointer to the buffer
- **Size**: Buffer size in bytes
- **EFA domain**: Associates the region with the network interface

Once registered, the EFA hardware can directly read from or write to this memory
without operating system involvement, enabling high-performance data transfers.

![alt MR](imgs/mr.png)

## Acknowledgments

Thanks to the [Perplexity blog post](https://www.perplexity.ai/hub/blog/high-performance-gpu-memory-transfer-on-aws) and the [asyncio](https://github.com/netcan/asyncio) C++ repository for inspiration.
