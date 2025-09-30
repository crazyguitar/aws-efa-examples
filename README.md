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
To understand this implementation, start with the [event loop](src/coro/include/coro.h)
to see how coroutines are scheduled.

The event loop has three key components:
- **Selector**: Queries I/O readiness (read/write events)
- **Schedule queue**: Holds coroutines waiting for events
- **Ready queue**: Contains coroutines ready for execution

Two main functions drive the loop:
- **Select()**: Uses the selector to check if events are ready. For EFA, this queries the [completion queue](https://github.com/ofiwg/libfabric/blob/7d60749967a425fff75dd36df03757ab871b0b4e/man/fi_cq.3.md) for fabric operation completion
- **RunOne()**: Checks if scheduled coroutines are ready, moving them to the ready queue for execution

![alt Event Loop](imgs/io.png)

## Acknowledgments

Thanks to the [Perplexity blog post](https://www.perplexity.ai/hub/blog/high-performance-gpu-memory-transfer-on-aws) and the [asyncio](https://github.com/netcan/asyncio) C++ repository for inspiration.
