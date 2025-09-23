# EFA Examples

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
