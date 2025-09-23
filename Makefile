.PHONY: all build

all: build

build:
	./build.sh

sqush:
	./enroot.sh -n efa -f ${PWD}/Dockerfile

clean:
	rm -rf build/

format:
	find . -type f -name "*.cc" -o -name "*.h" | xargs -I{} clang-format -style=file -i {}
