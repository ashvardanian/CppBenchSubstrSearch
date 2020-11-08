#!/usr/bin/env bash

# Run Cpp
./substr_search_cpp

# Run Python
python3 -OO -m py_compile substr_search.py &&
    python3 substr_search.py &&
    python3 __pycache__/substr_search.cpython-37.opt-2.pyc
