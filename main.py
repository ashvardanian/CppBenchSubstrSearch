from typing import Optional
import random
import time
import string


def next_offset(haystack: str, needle: str) -> Optional[int]:
    needle_len = len(needle)
    haystack_len = len(haystack)
    if needle_len > haystack_len:
        return None

    for off in range(haystack_len-needle_len):
        if haystack[off: needle_len] == needle:
            return off
    return None


def yield_matches(haystack: str, needle: str):
    progress = 0
    while True:
        off = next_offset(haystack[progress:], needle)
        if off is None:
            break
        yield progress + off
        progress += off


def random_str(n: int) -> str:
    # There is no 8-bit integer representation in Python.
    # The closest sibling of `std::vector<uint8_t>` is `str`.
    # return [random.randint(1, 255) for _ in range(n)]
    return ''.join(random.choice(string.ascii_lowercase) for _ in range(n))


def benchmark():
    haystack_size = 1 << 22
    haystack = random_str(haystack_size)
    needle_size = 6
    needles = [random_str(needle_size) for _ in range(20)]

    start = time.time()
    for needle in needles:
        for _ in yield_matches(haystack, needle):
            pass
    end = time.time()
    total_bytes = haystack_size * len(needles)
    duration = end - start
    print(f'Performance is: {total_bytes/duration} letters/sec')


if __name__ == "__main__":
    benchmark()
