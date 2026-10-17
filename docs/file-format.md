# File format additions

Pencil2D must still open every file. New data is optional attributes or
elements that Pencil2D skips; omit each when it holds the default.
Pencil2D drops groups and tweens if it re-saves the file.

| Data | Where | Owner |
|---|---|---|
| Layer blend mode | `<layer blend="N">`: 0 normal (omitted), 1 multiply, 2 screen, 3 overlay, 4 add. `Layer::createBaseDomElement`/`loadBaseDomElement` | #95 |
| Layer group | `<layerGroup id name collapsed visibility opacity/>` children of `<object>` (Pencil2D reads only `<layer>`); members carry `<layer group="id">`. One level, no nesting. Members are contiguous: a member outside the first run of its group loads ungrouped. Group list lives on `Object` | #93, #94 |
| Transform tween | `<image tween="1" easing="N" tx ty rot scale>` on bitmap and vector keys; fields in `KeyFrame`; `easing` is `CameraEasingType`; translate, rotate (degrees), uniform scale about the canvas origin. A key with `tween` interpolates to the next key on the frames it holds | #90 |
| Opacity tween | Same `tween` flag interpolates the existing `opacity` attribute | #91 |
| Mark | `<image mark="key\|breakdown\|inbetween">` on bitmap and vector keys (omitted when none; another value loads as none); `KeyFrame::mark()` | #186 |
| Rough layer | `<layer rough="1">` (omitted when off; another value loads as off); `Layer::isRough()`. Pencil2D shows and exports it as a normal layer | #192 |
| Lock-all | `<layer lockAll="1">` on every layer type (omitted when off; another value loads as off); `Layer::lockAll()`. Pencil2D ignores it | #199 |
| Frame markers | `<projectdata><markers><marker frame name color="#rrggbb"/></markers>`, in `ObjectData` | #92 |
| Lock transparency | `<layer lockAlpha="1">` on bitmap layers (omitted when off); field on `LayerBitmap` | #100 |
| Lip-sync markers | Frame markers with an optional `layer="id"` naming the sound layer they belong to (omitted for global markers); `layerId` on the marker struct in `ObjectData` | #108 |
| Ping-pong playback | `<projectdata><pingPong value="true"/>`, in `ObjectData`. Playback speed is not saved | #97 |
| Linked Drawing | `<image linkedTo="<src>">` on bitmap and vector keys: names the `src` of the earliest copy in the same layer (omitted on that copy and on unlinked keys). Each copy is still saved as a full image (ADR 0001), so Pencil2D opens independent copies. A `linkedTo` that names no image of the layer, or the image itself, is ignored. `KeyFrame::linkId()` | #194 |
| Paper template | `<projectdata><paperTemplate on="1" kind="genga" title id name state date notes/>` (omitted when off with every field empty; `on` other than `1` loads as off; a missing attribute loads empty). `ObjectData::paperTemplate()`. Pencil2D ignores it | #207 |
| Timesheet column | `<layer timesheet="KA".."KF"\|"IA".."IF"\|"CAM">`: Keyframe A-F, In-between A-F or Camera on the Timesheet (omitted for the default from layer order; another value loads as the default); `Layer::timesheetColumn()`, resolved by `effectiveTimesheetColumn`. Sound layers always fill Dialogue. Pencil2D ignores it | #209 |
| Storyboard layer | `<layer storyboard="1">` (omitted when off; another value loads as off); `Layer::isStoryboard()`. Panel fields: `<image scene cut notes audio>` on bitmap and vector keys, each omitted when empty; `KeyFrame::panelFields()`. A panel's Time is its Exposure, not saved. Pencil2D shows it as a normal layer | #211 |
