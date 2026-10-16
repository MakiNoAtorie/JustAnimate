# Rejected optimizations

Speed-ups that were tried and not kept, so they are not retried blindly.

- **#67 Pre-render composited frames ahead of the playhead.** Compositing
  stays on the GUI thread, so rendering ahead costs the same time as
  rendering on display. Back-to-back benchmarks showed no gain. Kept: the
  preload of frame data ahead of the playhead.
- **#72 Parallel PNG decode on open.** Frames decode lazily when first
  shown, so opening a project does no PNG work to parallelise.
- **#71 cost: reloading passed frames during playback.** Unloading frames
  far from the playhead makes playback reload them: 2.8 to 4.4 ms per frame.
  Accepted for the RAM saving; the #67 preload wins part of it back.
