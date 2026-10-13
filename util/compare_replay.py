#!/usr/bin/env python3
"""Compare two brush replay runs: speedup and pixel identity.

  pencil2d_tests "[.replay]" > before.txt     (or: cmake --build <build> --target replay)
  ... change the brush engine, rebuild ...
  pencil2d_tests "[.replay]" > after.txt
  python util/compare_replay.py before.txt after.txt

Reads the "replay <tool> <size> <ms> <layer sha1> <frame sha1>" lines. Exit code 1 if any
pixel hash differs or a case is missing in one run.
"""
import sys


def load(path):
    runs = {}
    with open(path) as f:
        for line in f:
            p = line.split()
            if len(p) == 6 and p[0] == "replay":
                runs[(p[1], int(p[2]))] = (float(p[3]), p[4], p[5])
    return runs


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    a, b = load(sys.argv[1]), load(sys.argv[2])
    if not a or not b:
        sys.exit("no 'replay' lines found; run the [.replay] test case")
    print(f"{'case':<14}{'before ms':>11}{'after ms':>11}{'speedup':>9}  pixels identical?")
    ok = True
    for key in sorted(set(a) | set(b)):
        name = f"{key[0]} {key[1]}"
        if key not in a or key not in b:
            print(f"{name:<14}{'-':>11}{'-':>11}{'-':>9}  MISSING in {'before' if key not in a else 'after'}")
            ok = False
            continue
        (t0, l0, f0), (t1, l1, f1) = a[key], b[key]
        same = l0 == l1 and f0 == f1
        ok &= same
        what = "yes" if same else "NO (" + ", ".join(n for n, x, y in (("layer", l0, l1), ("frame", f0, f1)) if x != y) + ")"
        print(f"{name:<14}{t0:>11.1f}{t1:>11.1f}{t0 / t1 if t1 else float('inf'):>8.2f}x  {what}")
    print("pixels identical: " + ("yes" if ok else "NO"))
    sys.exit(0 if ok else 1)


main()
