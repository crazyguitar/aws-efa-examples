# EFA Example: Send/Recv messages between ranks

```
# create a sqush file in project's root
make sqush

# build examples
make build

# salloc 2 nodes
salloc -N 2
./run.sh

# output example
# send data:[rank:0] [0] -> [1]
# send data:[rank:1] [1] -> [0]
# recv data: [rank:1] [1] -> [0]
# recv data: [rank:0] [0] -> [1]
```
