#!/usr/bin/env bash
# The executable will be placed in `bin/substr_search`

cmake -DCMAKE_BUILD_TYPE=Release . &&
    make
