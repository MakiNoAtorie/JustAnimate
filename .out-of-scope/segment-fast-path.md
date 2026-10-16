# Segment fast path for round tips

JustAnimate stamps dabs for every brush. It has no per-segment coverage mode (capsule SDF) for round tips.

## Why this is out of scope

A segment fast path (roadmap B11) is a second way to render round-tip strokes, kept in step with the dab path as a fallback. The lean rules add fast paths only after B0 measurements show the bottleneck; no measurement so far shows dab stamping at tight spacing as one.

If profiling shows dab stamping is the bottleneck for round tips, reopen B11 with those numbers.

## Prior requests

- #122: "Segment fast path for round tips (B11)"
