mkdir -p build && cd build &&
    cmake .. &&
    make &&
    bin/substr_search --benchmark_format=json --benchmark_out=../results/tmp.json

python -OO -m py_compile main.py &&
    python main.py &&
    python __pycache__/main.cpython-37.opt-2.pyc
