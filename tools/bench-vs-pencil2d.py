"""Black-box benchmark: JustAnimate vs Pencil2D through the command line.

Usage: python tools/bench-vs-pencil2d.py <justanimate.exe> <pencil2d.exe> <work dir> [reps]
Both exes need plugins/ffmpeg.exe next to them. See docs/benchmarks-vs-pencil2d.md.
"""
import ctypes
import ctypes.wintypes as wt
import random
import shutil
import statistics
import subprocess
import sys
import time
import zipfile
from pathlib import Path

from PIL import Image, ImageDraw

FRAMES, LAYERS = 500, 10


def make_project(path):
    """Same shape as the in-repo benchmark: 500 frames x 10 bitmap layers, one 100 px ellipse per key."""
    rng = random.Random(1)
    layers = ['<layer name="Camera Layer" type="5" width="1920" height="1080" id="1" visibility="1">'
              '<camera frame="1" dx="0" dy="0" r="0" s="1"/></layer>']
    with zipfile.ZipFile(path, "w") as z:
        z.writestr("mimetype", "application/x-pencil2d-pclx", zipfile.ZIP_STORED)
        for l in range(2, LAYERS + 2):
            colour = tuple(rng.randrange(256) for _ in range(3)) + (255,)
            img = Image.new("RGBA", (100, 100))
            ImageDraw.Draw(img).ellipse((0, 0, 99, 99), fill=colour)
            tmp = path.with_suffix(".png")
            img.save(tmp)
            png = tmp.read_bytes()
            keys = []
            for f in range(1, FRAMES + 1):
                src = f"{l:03d}.{f:03d}.png"
                z.writestr("data/" + src, png, zipfile.ZIP_DEFLATED)
                keys.append(f'<image frame="{f}" topLeftX="{rng.randrange(-640, 540)}" '
                            f'topLeftY="{rng.randrange(-360, 260)}" opacity="1" src="{src}"/>')
            layers.append(f'<layer name="Layer {l}" type="1" id="{l}" visibility="1">{"".join(keys)}</layer>')
            tmp.unlink()
        z.writestr("main.xml", '<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE PencilDocument><document>'
                   '<projectdata><fps value="24"/></projectdata>'
                   f'<object>{"".join(layers)}</object><version>0.6.6</version></document>',
                   zipfile.ZIP_DEFLATED)


class Counters(ctypes.Structure):
    _fields_ = [("cb", wt.DWORD), ("PageFaultCount", wt.DWORD)] + \
               [(n, ctypes.c_size_t) for n in ("PeakWorkingSetSize", "WorkingSetSize", "QuotaPeakPagedPoolUsage",
                                               "QuotaPagedPoolUsage", "QuotaPeakNonPagedPoolUsage",
                                               "QuotaNonPagedPoolUsage", "PagefileUsage", "PeakPagefileUsage")]


def run(exe, args):
    """Returns (seconds, peak working set MB) of one process run; raises on a non-zero exit."""
    t = time.perf_counter()
    p = subprocess.Popen([str(exe)] + args, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    _, err = p.communicate()
    dt = time.perf_counter() - t
    c = Counters(cb=ctypes.sizeof(Counters))
    ctypes.windll.psapi.GetProcessMemoryInfo(wt.HANDLE(int(p._handle)), ctypes.byref(c), c.cb)
    if p.returncode:
        raise RuntimeError(f"{exe.name} {args} exited {p.returncode}: {err.decode(errors='replace')[-500:]}")
    return dt, c.PeakWorkingSetSize / 2**20


def main():
    apps = {"JustAnimate": Path(sys.argv[1]), "Pencil2D": Path(sys.argv[2])}
    work = Path(sys.argv[3])
    reps = int(sys.argv[4]) if len(sys.argv) > 4 else 3
    work.mkdir(parents=True, exist_ok=True)
    big = work / "bench.pclx"
    if not big.exists():
        make_project(big)
    small = work / "small.pclx"
    if not small.exists():
        with zipfile.ZipFile(big) as zi, zipfile.ZipFile(small, "w") as zo:
            zo.writestr("mimetype", zi.read("mimetype"), zipfile.ZIP_STORED)
            zo.writestr("main.xml", '<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE PencilDocument><document>'
                        '<object><layer name="Camera Layer" type="5" width="1920" height="1080" id="1" visibility="1">'
                        '<camera frame="1" dx="0" dy="0" r="0" s="1"/></layer></object>'
                        '<version>0.6.6</version></document>')
    hd = ["--width", "1920", "--height", "1080"]
    # (name, project, output file name, extra args, expected output files)
    scenarios = [
        ("startup", small, "f.png", ["--start", "1", "--end", "1"], 1),
        ("open + 1 frame", big, "f.png", hd + ["--start", "1", "--end", "1"], 1),
        ("PNG sequence", big, "f.png", hd + ["--start", "1", "--end", str(FRAMES)], FRAMES),
        ("MP4", big, "m.mp4", hd + ["--start", "1", "--end", str(FRAMES)], 1),
        ("GIF 720p", big, "m.gif", ["--width", "1280", "--height", "720", "--start", "1", "--end", str(FRAMES)], 1),
    ]
    print(f"{FRAMES} frames x {LAYERS} bitmap layers, {reps} reps after one warm-up, fresh process per run")
    print(f"{'scenario':<16}{'app':<13}{'median s':>10}{'min s':>9}{'max s':>9}{'peak MB':>9}")
    for name, project, out, extra, expect in scenarios:
        results = {a: [] for a in apps}
        for rep in range(reps + 1):
            for app, exe in apps.items():  # interleaved, so drift hits both apps alike
                d = work / "out"
                shutil.rmtree(d, ignore_errors=True)
                d.mkdir()
                r = run(exe, [str(project), "-o", str(d / out)] + extra)
                got = len(list(d.iterdir()))
                if got != expect:
                    raise RuntimeError(f"{app} {name}: {got} output files, expected {expect}")
                if rep:
                    results[app].append(r)
        for app, rs in results.items():
            ts = [r[0] for r in rs]
            print(f"{name:<16}{app:<13}{statistics.median(ts):>10.2f}{min(ts):>9.2f}{max(ts):>9.2f}"
                  f"{statistics.median(r[1] for r in rs):>9.0f}")


if __name__ == "__main__":
    main()
