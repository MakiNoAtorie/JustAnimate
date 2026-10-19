# Layers

This chapter shows how to use layers. A layer is one level of the picture. You put the parts of a scene on different layers, so that you can change one part and not the others.

The layers are in the **Timeline** Panel. Each layer has one row. The top row is the top layer: it is in front of the layers below it. See [Timeline and canvas](01-getting-started.md#getting-started-timeline-canvas) for the other parts of the Timeline.

## Layer types {#layers-types}

![The Timeline with the layers of the demo project](images/layers-timeline.png)

JustAnimate has five kinds of layer. The icon in the row shows the kind.

- **Bitmap.** Draw with pixels. Use it for pencil and brush drawings, and for painted colour.
- **Vector.** Draw with lines that you can change after you draw them.
- **Camera.** Sets the part of the picture that is exported and the way it moves. A project must have at least one Camera layer.
- **Sound.** Holds sound clips. It has no picture.
- **Storyboard.** A layer whose Drawings are storyboard panels. See [Storyboard layers](#layers-storyboard).

Only Bitmap and Vector layers hold Drawings that you draw. A Drawing stays on the screen until the next Drawing on the same layer.

Each row has these controls, from left to right:

- The **eye** shows or hides the layer.
- The **onion skin** toggle turns the onion skin on or off for the layer. A Sound layer does not have it.
- The **layer icon** shows the kind of layer.
- The **Lock Transparency** toggle. Only Bitmap layers have it. See [Lock Transparency](#layers-lock-transparency).
- The **lock** toggle locks the whole layer. See [Lock a layer](#layers-lock-all).
- The **name** of the layer.

The current layer is the layer that you draw on. Click a layer name to make it the current layer. You can also press the Up and Down arrow keys while the pointer is over the Timeline.

### Storyboard layers {#layers-storyboard}

A Storyboard layer is a layer whose Drawings are the panels of a storyboard. The Exposure of a panel is its time on the screen, so playback shows the storyboard as a timed film.

1. Choose Layer > New Storyboard Layer. JustAnimate adds a layer that is ready for panels.
2. To change a layer that you already have, right-click its name and choose **Storyboard Layer**. Choose it again to change the layer back.
3. Open the **Storyboard** Panel. It has one row for each panel, with the text fields Scene, Cut, Notes and Audio, and the Time of the panel. Time is the Exposure of the panel, in seconds and frames.

![The Storyboard Panel](images/layers-storyboard.png)

If the project has no Storyboard layer, the Storyboard Panel shows the button **Make current layer the Storyboard layer**.

## Add, rename, duplicate and delete {#layers-edit}

### Add a layer {#layers-add}

1. In the Timeline, click the **Add Layer** button (the plus sign above the layer names).
2. Choose **New Bitmap Layer**, **New Vector Layer**, **New Sound Layer**, **New Storyboard Layer** or **New Camera Layer**.

![The Add Layer menu](images/layers-add-menu.png)

You can also use the Layer menu. It has the same five commands.

The new layer goes above the others and becomes the current layer. Its name is the kind of layer, for example "Bitmap Layer". When that name is in use, JustAnimate adds a number: "Bitmap Layer 2".

- A new **Camera** layer first opens a box where you set the name and the size (Width and Height) of the camera.
- To put sound on a layer, choose File > Import > Sound.... If the current layer is not a Sound layer, JustAnimate asks if it must make one.

### Rename a layer {#layers-rename}

You change the name in the row. Do one of these:

- Double-click the name of the layer.
- Make the layer current and press F2.
- Choose Layer > Rename Layer.

A box with the name opens in the row. Type the new name and press Enter. Press Esc to cancel. An empty name is not accepted.

![The name of a layer in the row, ready for a new name](images/layers-rename.png)

### Duplicate a layer {#layers-duplicate}

Click the **Duplicate Layer** button (the two sheets above the layer names). JustAnimate makes a copy of the current layer with all its Drawings and settings. The copy is named like the original and ends with "(copy)", for example "Background (copy)".

### Delete a layer {#layers-delete}

Click the **Delete Layer** button (the minus sign above the layer names), or choose Layer > Delete Current Layer.

JustAnimate does not delete these layers. The button and the menu item are not available for them:

- the bottom layer of the project
- the last Camera layer

### Undo {#layers-undo}

All of these changes are in the undo history. To go back, choose Edit > Undo (Ctrl+Z). One undo brings back a deleted layer with all its Drawings. One undo also removes a layer that you added or duplicated. Choose Edit > Redo to do the change again.

## Groups {#layers-groups}

A group holds layers that belong together, for example the rough and the line of one character. The layers of a group are next to each other. You can fold the group, hide it, and set one opacity for all its layers.

To make a group, right-click the name of a layer and choose **New Group**. The layer goes into a new group named "Group 1". The next group is "Group 2". To put more layers in the group, right-click the name of another layer, choose **Add to Group** and then the name of the group.

![A group named Ball with two layers](images/layers-group.png)

The row of the top layer of a group shows the name of the group, then a slash, then the name of the layer. The other layers of the group have a space in front of the name. When the group is not at full opacity, the percent shows after the group name.

To change a group, right-click the name of one of its layers. The menu has these commands:

- **Remove from Group.** Takes the layer out of the group. When the last layer leaves, the group is deleted.
- **Collapse Group** and **Expand Group.** Fold the group into one row, or open it again. You can also click the small arrow in front of the group name.
- **Show Group.** Tick it to show the group. Clear it to hide the whole group. A hidden group shows "(hidden)" after its name.
- **Group Opacity....** A box opens. Type a percent from 0 to 100 in **Opacity (%):** and click OK. The layers of the group are then shown as one picture at that opacity. The layers do not show through each other.

![The Ball group collapsed, at 50 percent opacity](images/layers-group-collapsed.png)

Group opacity does not change the Drawings. A Rough layer in a group is faded by itself and is not part of the group picture. See [Rough layers](#layers-rough).

## Blend modes {#layers-blend}

A blend mode sets how a layer mixes its colours with the layers below it. Bitmap and Vector layers have a blend mode.

1. Right-click the name of the layer.
2. Choose **Blend Mode**.
3. Choose one of the modes.

![The Blend Mode list](images/layers-blend-modes.png)

The modes are:

- **Normal.** The layer covers the layers below it. This is the default.
- **Multiply.** The colours get darker where the layers overlap. Use it for shadows.
- **Screen.** The colours get lighter where the layers overlap. Use it for glow.
- **Overlay.** Makes the dark parts darker and the light parts lighter, by the colours below.
- **Add.** Adds the colours to the colours below. The result is lighter.

The blend mode is part of the layer. It shows in the canvas and in the exported pictures.

The same menu has the commands for Lock Transparency, Rough Layer, Storyboard Layer, Timesheet Column and the groups.

## Lock Transparency {#layers-lock-transparency}

Lock Transparency keeps the transparent pixels of a Bitmap layer transparent. You can then change the colour of a drawing and not draw outside it.

To turn it on, click the **Lock Transparency** toggle in the row of the layer. You can also right-click the name and choose **Lock Transparency**. Click the toggle again to turn it off. Vector layers do not have it.

![Lock Transparency is on for the Background layer](images/layers-lock-transparency.png)

When Lock Transparency is on:

- A stroke changes only the pixels that already have paint. A stroke on an empty part does nothing.
- The Eraser does not remove paint.
- A fill with the Bucket does not paint on transparent pixels.

## Rough layers {#layers-rough}

A Rough layer is for rough drawings. JustAnimate shows it tinted and faded, under the other layers. A Rough layer is never exported. Use it for the first loose drawings, then draw the clean lines on a layer above it.

To make a layer a Rough layer, right-click its name and choose **Rough Layer**. The word "ROUGH" shows in the row. Choose the command again to change the layer back.

![A Rough layer under the clean layers](images/layers-rough.png)

A Rough layer can be a Bitmap layer or a Vector layer. Use Marks to show which Drawings are the Keys, Breakdowns and Inbetweens of the rough motion.

## Solo a layer {#layers-solo}

Solo shows one layer and hides the other drawing layers. Use it to check one layer alone.

1. Hold Alt and click the **eye** of the layer.
2. To go back, hold Alt and click the same eye again. All layers return to the state they had before.

![Only the Rough layer is shown](images/layers-solo.png)

Solo hides the Bitmap and Vector layers. It does not change Camera and Sound layers. A plain click on the eye still shows or hides one layer.

## Lock a layer {#layers-lock-all}

A locked layer cannot be changed. Click the **lock** toggle in the row. The lock icon is filled when the layer is locked. Click it again to unlock the layer.

A locked layer refuses all edits to its Drawings. This includes drawing, erasing, paste, clear, move and the change of the exposure.

## Refused edits {#layers-refused}

JustAnimate refuses an edit when the layer is hidden or locked. It does not change the layer. Instead, it tells you why:

- A message shows in the status bar at the bottom of the window for a few seconds: `Layer "Rough" is locked` or `Layer "Rough" is hidden`.
- The eye or the lock of the layer flashes in its row.

![The status bar says that the layer is locked](images/layers-locked-notice.png)

To fix this, show the layer (click the eye) or unlock it (click the lock). Then do the edit again.
