# GPU brush path

JustAnimate computes strokes on the CPU. There is no GPU-resident stroke buffer.

## Why this is out of scope

Research for the roadmap (B10) found that Photoshop, Clip Studio and Krita all compute strokes on the CPU and use the GPU for display only, and Krita's 2018 OpenCL brush test was about 30 % slower because of per-dab transfers. A GPU path would be a second implementation of every brush, with the CPU kept as the reference, for a gain no one has measured. The lean rules add fast paths only after measurements show the bottleneck.

If a measured stroke bottleneck appears that the CPU path cannot fix, reopen B10; GPU display comes before GPU stroke maths.

## Prior requests

- #121: "GPU brush path (B10)"
