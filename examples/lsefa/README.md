# EFA Example: List all EFA information on a host

```
# launch an interactive enroot environment
enroot create --name efa efa+latest.sqsh
enroot start --mount /fsx:/fsx efa /bin/bash

# build examples
make build

# run the binary
./build/examples/lsefa/lsefa
```
