from typing import Optional
import random
import time
import string

haystack_size = 1024 * 1024 * 16
needle_size = 10


def yield_matches(haystack: str, needle: str):

    needle_len = len(needle)
    haystack_len = len(haystack)
    if needle_len > haystack_len:
        return

    for off in range(haystack_len-needle_len):
        if haystack[off: needle_len] == needle:
            yield off


def random_str(n: int, rich: bool) -> str:
    # There is no 8-bit integer representation in Python.
    # The closest sibling of `std::vector<uint8_t>` is `str`.
    # return [random.randint(1, 255) for _ in range(n)]
    poor_ascii = string.ascii_lowercase
    rich_ascii = string.ascii_lowercase + string.ascii_uppercase
    return ''.join(random.choice(rich_ascii if rich else poor_ascii) for _ in range(n))


def benchmark(rich: bool):
    haystack = random_str(haystack_size, rich)
    needles = [random_str(needle_size, rich) for _ in range(20)]

    start = time.time()
    cnt_matches = 0
    for needle in needles:
        for _ in yield_matches(haystack, needle):
            cnt_matches += 1
    end = time.time()
    total_bytes = haystack_size * len(needles)
    duration = end - start

    print(f'- bytes/s: {int(total_bytes/duration):,}')
    print(f'- matches/s: {int(cnt_matches/duration):,}')


if __name__ == "__main__":
    print('----------------------------------------------------------------------------------------------------------------')
    print('Python Benchmark')
    print('----------------------------------------------------------------------------------------------------------------')
    print('Poor Strings: [a-z]')
    benchmark(False)
    print('Rich Strings: [A-Za-z]')
    benchmark(True)
