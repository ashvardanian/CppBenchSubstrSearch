# #!/usr/bin/env bash
# Source: https://github.com/google/benchmark#installation
# Instead of building from source this surprisingly huge library
# one can run: `apt install libbenchmark-dev`, but it comes with
# the "DEBUG" version of the library.

git clone https://github.com/google/benchmark.git
git clone https://github.com/google/googletest.git benchmark/googletest
cd benchmark
cmake -E make_directory "build"
cmake -E chdir "build" cmake -DCMAKE_BUILD_TYPE=Release ../
cmake --build "build" --config Release --target install
cd ..
