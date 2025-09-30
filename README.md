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

## Acknowledgments

Thanks to the [Perplexity blog post](https://www.perplexity.ai/hub/blog/high-performance-gpu-memory-transfer-on-aws) and the [asyncio](https://github.com/netcan/asyncio) C++ repository for inspiration.
