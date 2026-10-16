# MyPaint brush engine

JustAnimate does not embed libmypaint and does not load `.myb` brushes.

## Why this is out of scope

The brush engine is one module with plain classes, on purpose: the lean rules say "no engine interface until B9 (MyPaint) actually needs a second engine". Adding libmypaint means a second engine behind the `DabCompositor` surface, a new dependency to build on three platforms, and a second brush format to support in presets. Nobody has asked to use MyPaint brushes.

If users ask for it, reopen roadmap item B9; that is also the point to introduce an engine interface.

## Prior requests

- #120: "MyPaint brushes via libmypaint (B9)"
