.PHONY: all build

all: build

build:
	./build.sh

sqush:
	./enroot.sh -n efa -f ${PWD}/Dockerfile

clean:
	rm -rf build/
