#!/usr/bin/env bash

# Run Cpp
./substr_search_cpp

# Run Python
python -OO -m py_compile substr_search.py &&
    python substr_search.py &&
    python __pycache__/substr_search.cpython-37.opt-2.pyc
