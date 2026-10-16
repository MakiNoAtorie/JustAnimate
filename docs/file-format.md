# File format additions

Pencil2D must still open every file. New data is optional attributes or
elements that Pencil2D skips; omit each when it holds the default.

| Data | Where | Owner |
|---|---|---|
| Layer blend mode | `<layer blend="N">`: 0 normal (omitted), 1 multiply, 2 screen, 3 overlay, 4 add. `Layer::createBaseDomElement`/`loadBaseDomElement` | #95 |
| Layer group | `<layerGroup id name collapsed visibility opacity/>` children of `<object>` (Pencil2D reads only `<layer>`); members carry `<layer group="id">`. One level, no nesting. Group list lives on `Object` | #93, #94 |
| Transform tween | `<image tween="1" easing="N" tx ty rot scale>` on bitmap and vector keys; fields in `KeyFrame`; `easing` is `CameraEasingType`; translate, rotate (degrees), uniform scale about the canvas origin. A key with `tween` interpolates to the next key on the frames it holds | #90 |
| Opacity tween | Same `tween` flag interpolates the existing `opacity` attribute | #91 |
| Frame markers | `<projectdata><markers><marker frame name color="#rrggbb"/></markers>`, in `ObjectData` | #92 |
| Ping-pong playback | `<projectdata><pingPong value="true"/>`, in `ObjectData`. Playback speed is not saved | #97 |
