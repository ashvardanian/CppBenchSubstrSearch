#!/usr/bin/env bash

# Run Cpp
./substr_search_cpp

# Run Python
python3 -OO -m py_compile substr_search.py &&
    python3 substr_search.py &&
    python3 __pycache__/substr_search.cpython-37.opt-2.pyc

# Run JS
# https://stackoverflow.com/a/54675887
node --max-old-space-size=4096 substr_search.js
