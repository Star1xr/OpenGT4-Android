"""Magnitude of the differences between two captures (standard library).

Usage: python tests/ui/png_stats.py A.png B.png
Prints the changed pixel count, the mean and 99th percentile of the summed
RGB difference over changed pixels, and how many differ by more than 48.
"""
import sys

from png_diff import read_png


def main():
    _, _, a = read_png(sys.argv[1])
    _, _, b = read_png(sys.argv[2])
    d = sorted(sum(abs(p - q) for p, q in zip(pa, pb)) for ra, rb in zip(a, b) for pa, pb in zip(ra, rb) if pa != pb)
    if not d:
        print("identical")
        return
    print(f"{len(d)} px differ, mean {sum(d) / len(d):.1f}, p99 {d[int(len(d) * .99)]}, >48: {sum(1 for x in d if x > 48)}")


if __name__ == "__main__":
    main()
