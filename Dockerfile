# The fist stage is needed to compile the C++ code.
FROM gcc
RUN apt-get update && apt-get -y install cmake && apt-get clean && rm -rf /var/lib/apt/lists/*
COPY . /substr_search
WORKDIR /substr_search/
RUN "./scripts/install_dependencies.sh"
RUN "./scripts/build.sh"

# The second stage is responsible for running benchmarks.
FROM python
WORKDIR /root/
COPY --from=0 /substr_search/substr_search_cpp substr_search_cpp
COPY substr_search.js substr_search.js
COPY substr_search.py substr_search.py
COPY scripts/bench.sh bench.sh
CMD ["./bench.sh"]  
