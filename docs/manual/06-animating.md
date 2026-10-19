# Animating {#animating}

This chapter describes the features that help you to make motion: the onion
skin, Shift and Trace, Flip, playback, Key Transform and tweens, Create
Inbetweens, the camera layer and peg bar alignment. It is in the order that
you meet the features when you animate.

Most of these features use the **Timeline** Panel and the **Onion skin**
Panel. Choose the Animate Layout to see them. To learn about Layouts, read
[Layouts](01-getting-started.md#getting-started-layouts).

## Onion skin {#animating-onion}

The onion skin shows the neighbour Drawings of the current Drawing in a faint
tint. Use it to put a new Drawing in the right place, and to see how far the
shape moves from one Drawing to the next.

![The onion skin around Drawing 7: the previous Drawings in red, the next Drawings in blue](images/animating-onion-canvas.png)

The current Drawing shows in full colour. The previous Drawings use a red
tint. The next Drawings use a blue tint. A Drawing that is far from the
current Drawing is fainter than a Drawing that is near.

### Turn the onion skin on and off {#animating-onion-toggle}

- Press O, or choose **View > Onion Skin > Onion Skin**. This turns the previous
  and the next onion skin on or off together.
- Choose **View > Onion Skin > Previous** or **View > Onion Skin > Next** to
  turn only one direction on or off.
- Press Ctrl+8 to show or hide the **Onion skin** Panel.

### The Onion skin Panel {#animating-onion-panel}

The Onion skin Panel has all the settings. It is a tab under the Timeline in
the Animate Layout. You can also choose **Window > Onion skin**.

![The Onion skin Panel](images/animating-onion-panel.png)

- **Previous Frames** and **Next Frames.** The check box of each group turns that
  direction on or off. The number box sets how many onion skins show in that
  direction, from 1 to 60.
- **Tint buttons.** The button with the stack icon turns the tint of that
  direction on or off. The coloured square opens a colour dialog, where you
  choose the tint colour. The default tints are red for the previous
  direction and blue for the next direction.
- **Distributed Opacity.** **Max** is the opacity of the onion skin that is
  nearest to the current Drawing. **Min** is the opacity of the onion skin that
  is farthest from it. The skins between them change step by step.
- **Falloff.** The list next to Max and Min sets how the opacity changes from
  Max to Min with the distance: **Linear**, **Gentle** or **Steep**. Use
  Steep when you want only the nearest Drawings to show clearly.
- **Show On All Layers.** When this is off, the onion skin shows only the
  current layer. When it is on, the onion skin shows every layer.
- **Show During Playback.** When this is off, the onion skin hides while the
  animation plays.

### The three modes {#animating-onion-modes}

The list under Show On All Layers sets which Drawings show as onion skins.

- **Every frame.** The onion skin shows the frames that are next to the
  playhead, one for each frame number. A Drawing with an Exposure of more
  than one frame shows more than once.
- **Every drawing.** The onion skin shows the neighbour Drawings of the
  layer. Each Drawing shows once, and the Exposure does not matter. This is
  the mode of the picture above.
- **Marked drawings.** The onion skin shows only the Drawings that have
  the chosen Marks.

A Mark is a label on a Drawing. It tells the role of the Drawing in the
motion: **Key**, **Breakdown** or **Inbetween**. To set a Mark, put the
playhead on a Drawing and choose **Animation > Mark**, then the Mark. The
shortcuts are Alt+K for Key, Alt+B for Breakdown, Alt+I for Inbetween and
Alt+0 for Clear Mark. The Timeline shows a diamond for a Key, a triangle
for a Breakdown and a small dot for an Inbetween.

In the Marked drawings mode the three check boxes **Key**, **Breakdown**
and **Inbetween** turn on. Select the Marks that you want to see. In the
picture below, only the Drawings that are marked Key show as onion skins.
The Drawings with other Marks are hidden. This is a quick way to compare
the main poses.

![The Marked drawings mode with only Key selected](images/animating-onion-marked.png)

![The Onion skin Panel in the Marked drawings mode](images/animating-onion-marked-panel.png)

### The onion skin of one layer {#animating-onion-layer}

Each layer has its own onion skin switch. It is the stack icon in the row of
the layer in the Timeline, between the visibility dot and the layer type
icon. Click it to turn the onion skin of that layer on or off. A layer with
the onion skin off shows a grey icon. A sound layer has no switch.

![The Timeline with the onion skin off on the Background layer](images/animating-onion-layer.png)

The switch of a layer matters in two cases. The onion skin of the current
layer shows only when its switch is on. With Show On All Layers on, only the
layers with their switch on show an onion skin.

## Shift and Trace {#animating-shift-trace}

Shift and Trace moves and turns the onion skins on the canvas. Use it to trace
an Inbetween when the neighbour Drawings are not in the place that you
need. It changes only how the onion skins show. It never changes a Drawing.

To use it, do the steps that follow.

1. Turn the onion skin on, and make sure that the Drawings to move show as
   onion skins.
2. Choose **View > Onion Skin > Shift and Trace**, or press Shift+T.
3. Put the playhead on the Drawing that you want to draw.

Each onion skin now has two round handles, joined by a line.

![Shift and Trace: the next Drawing is moved and turned, the previous Drawing is not](images/animating-shift-trace.png)

- Drag the lower handle to move the onion skin.
- Drag the upper handle to turn the onion skin around its centre.

Shift and Trace works on bitmap layers and on vector layers. The handles
do not show while the animation plays. The offsets are only for your
view: they are not saved, and the Drawings do not change. When you move the
playhead to another frame, change the current layer or turn Shift and Trace
off, the onion skins return to their true places.

## Flip {#animating-flip}

Flip shows Drawings one after another by hand, to check the motion while you
draw. JustAnimate has three ways to flip.

### Flip with the pen {#animating-flip-pen}

Pen-drag Flip uses a key and the pen.

1. Hold the Flip key. The default key is F. You can change it in
   **Edit > Preferences... > Shortcuts**, in the entry
   "Flip (hold and drag the pen)".
2. Press the pen on the canvas and drag it to the right or to the left.

![Pen-drag Flip: the playhead is on another Drawing than at the start](images/animating-flip.png)

Each 20 pixels of the drag move the playhead by one Drawing of the current
layer. A drag to the right goes forward. A drag to the left goes back. The
pen does not draw while the Flip key is down. When you release the key, the
playhead goes back to the frame where you started. This lets you check a
motion as many times as you want.

In the Marked drawings mode of the onion skin, Flip steps only through the
Drawings that have the chosen Marks.

### Flip In-Between and Flip Rolling {#animating-flip-roll}

Two commands in the **Animation** menu play a short set of Drawings.

- **Flip In-Between** (Alt+Z) plays the previous Drawing, the current
  Drawing and the next Drawing of the current layer, then goes back to the
  current frame. The playhead must be between two Drawings.
- **Flip Rolling** (Alt+X) plays the last Drawings before the current
  Drawing, then the current Drawing.

Set the speed and the number of Drawings in **Edit > Preferences... >
Timeline**, in the group **Flip and Roll**.

## Playback {#animating-playback}

Press Ctrl+Return, choose **Animation > Play**, or click **Play** in the
Timeline to start the animation. Click the button again to stop. The
Timeline has the playback controls.

![The playback controls in the Timeline](images/animating-playback.png)

- **Jump to the Start** and **Jump to the End** move the playhead to the first
  and the last frame.
- **Loop** (Ctrl+L) makes the animation start again at the end. The button
  is highlighted when Loop is on.
- **fps.** The box sets the frames per second.
- **Playback speed.** The list sets the speed: 0.5x, 1x or 2x. The fps
  of the project does not change.
- **Ping-pong.** The animation plays forward to the end, then back to the
  start, and repeats until you stop it.
- **Range, Start and End.** See
  [Play-around and the loop range](#animating-range).
- **Sound on/off** and **Sound scrub on/off** control the sound.

A Drawing stays on screen for its whole Exposure while the animation plays.

### Step with the keys {#animating-step-keys}

The Next Frame key (.) and the Previous Frame key (,) move the playhead by one
frame. Hold a key down to flip through the frames. The playhead moves at the
fps of the project, not at the repeat speed of the keyboard. The keys
Alt+. and Alt+, move the playhead to the next and the previous Drawing.

## Play-around and the loop range {#animating-range}

Play-around plays only the frames near the playhead. It is a fast way to check
the Drawing that you work on.

1. Set the number in the box with the plus-minus sign, next to the
   Play-around button. For example, ±3.
2. Press Alt+Return, choose **Animation > Play Around Playhead**, or click the
   button with the two arrows.

The animation loops from three frames before the playhead to three frames
after it. Press Alt+Return again to stop. The playhead returns to the frame
where you started.

The loop range is a part of the animation that plays by itself.

![The loop range from frame 4 to frame 10, with Play-around set to 3](images/animating-loop-range.png)

- Turn on **Range** in the Timeline, and type the first frame and the last
  frame in the two boxes next to it. The boxes are off when Range is off.
  You can also choose **Animation > Range**.
- Or hold Shift and drag on the number ruler of the Timeline. This sets the
  range and turns on Range and Loop.

When Range is on, **Play** plays only the frames of the range. The range is
saved in the project.

## Key Transform and tweens {#animating-tween}

A tween moves, turns, scales or fades a whole Drawing between two Drawings. The
program makes the tween. You do not draw it. A tween is useful for a slide,
a zoom or a fade of one Drawing, because you do not need to draw each
frame.

A tween has two parts: the transform of each Drawing, and the tween switch.

### Set the transform of a Drawing {#animating-tween-transform}

1. Select a bitmap layer or a vector layer, and put the playhead on a Drawing.
2. Select the Move tool and turn on **Transform Key** in the Tool settings
   Panel.
3. Drag on the canvas to move the Drawing. Hold Ctrl and drag to turn it.
   Hold Shift and drag to scale it. You can also type values in the boxes
   **Move X**, **Move Y**, **Rotation**, **Scale** and **Opacity**.

![A Drawing with a Transform Key of 80, -30, 20 degrees and 130 percent](images/animating-transform-key.png)

[Transform Key](02-drawing-tools.md#drawing-tools-transform-key) in the
chapter on the drawing tools explains the same controls. Do the steps again
on the next Drawing to give it its own transform.

### Tween to the next Drawing {#animating-tween-switch}

1. Right-click the cell of the first Drawing in the Timeline or in the
   X-sheet. A menu opens.
2. Select **Tween to Next Key**. A check mark shows that it is on.
3. To change the easing, select **Tween Easing**, then an easing in the list.

![The cell menu of a Drawing: Tween to Next Key and the list of easings](images/animating-tween-menu.png)

The frames that the first Drawing holds now go step by step from its
transform to the transform of the next Drawing on the layer. The easing sets
how the speed changes. Each easing in the list has a small picture of its
curve. **Linear** has a constant speed. The names that end in
**Ease-in** start slowly. The names that end in **Ease-out** end slowly.
The other names do both, or go past the end value and come back, such as
**Overshoot**, **Elastic** and **Bounce**.

![At frame 4 the first Drawing is halfway to the transform of Drawing 7](images/animating-tween.png)

The tween uses the Drawings as they are. It does not change the Drawing
images. To stop a tween, select **Tween to Next Key** again to clear it.

## Create Inbetweens {#animating-inbetweens}

Create Inbetweens draws the Drawings between two vector Drawings. It moves each
point of each stroke from its place in the first Drawing to its place in the
second Drawing. The easing sets the speed.

Before you start, check these points.

- The current layer is a vector layer.
- The two Drawings have the same number of strokes. The program pairs the
  strokes in the order that they were drawn.
- There are empty frames between the two Drawings. The command fills only
  the empty frames. It never replaces a Drawing.

To make the Inbetweens, do the steps that follow.

1. In the Timeline, click the first Drawing. Then Ctrl+click the second
   Drawing. Exactly two Drawings must be selected.
2. Choose **Animation > Create Inbetweens...**, or press Ctrl+Shift+I.
3. A menu of easings opens at the pointer. The easing that you used last is
   selected. Choose an easing, or press Enter to accept the selected one.

![The menu of easings of Create Inbetweens](images/animating-inbetween-easing.png)

Each new Drawing has the Mark **Inbetween**. You can undo the command in one
step. On a bitmap layer the command shows a message, because it works only
on vector layers. The command also shows a message when you did not select
two Drawings, when the Drawings have a different number of strokes, and
when there is no empty frame between them.

![The new Drawings on frames 2 to 6, with the onion skin](images/animating-inbetweens.png)

The new Drawings are normal Drawings. Change them with the drawing tools to
fix the motion. A tween (see the section before) is different: it changes
only the transform and keeps one image.

## The camera layer {#animating-camera}

A camera layer holds the camera of the project. The camera frame sets what the
movie shows. When the camera moves from one key to the next, the movie shows a
pan, a zoom or a turn. A project always has at least one camera layer.

- To add a camera layer, choose **Layer > New Camera Layer** (Ctrl+Alt+C).
  A dialog, **Camera Properties**, asks for the name and the size of the
  camera.
- To change the name or the size later, double-click the camera icon in the
  row of the layer.

### Move the camera {#animating-camera-move}

Select the camera layer. The Move button of the Tool column then selects the
Camera tool.

![The Camera tool and its Tool settings](images/animating-camera.png)

1. Put the playhead on a frame that has a camera key. Add a key with **New
   drawing** in the Timeline.
2. Drag inside the camera frame to move it.
3. Drag a corner of the frame to scale it.
4. Drag the round handle above the frame to turn it. Hold Shift to turn in
   steps.

The camera changes only on a frame that has a camera key. Between two keys,
the camera moves by itself.

The Tool settings Panel has these controls:

- **Reset all** puts the camera at the current frame back to the normal
  state. The three buttons next to **Reset** reset only the position, the
  rotation or the scale.
- **Show path** shows the path of the camera between the keys. The list next
  to it sets the colour of the path.
- **Reset path** makes the path straight again.

### The path and the easing {#animating-camera-path}

On a frame between two camera keys, the path shows as a row of dots. A square
handle on the path has a label with the name of the easing. Drag the handle
to bend the path.

![The camera path between two keys, with the label of the easing](images/animating-camera-path.png)

To change the easing, right-click the cell of the first camera key.

![The cell menu of a camera key, with the Transform submenu](images/animating-camera-menu.png)

- **Easing: frame A to B** sets how the speed changes between the two keys.
  Select **Linear** or one of the groups In, Out, In-Out and Out-In.
- **Transform** has **Reset all**, **Reset position**, **Reset scale** and
  **Reset rotation** for this key. It also has **Align horizontally to
  frame**, **Align vertically to frame** and **Hold to frame**. Use
  **Hold to frame** to keep the camera still until the next key.

The camera of the project is the one that the export uses. In the demo above,
the camera moves from frame 1 to frame 24.

## Peg bar alignment {#animating-pegbar}

A scanned Drawing sits on paper with a peg hole. The hole must be in the
same place in each scan. If it is not, the Drawing jumps on the screen.
Peg bar alignment moves each Drawing so that its peg hole is in the same pixel
as the peg hole of a reference Drawing. It works on bitmap layers only.

1. Select the bitmap layer of the reference Drawing and put the playhead on
   it. This Drawing is the reference.
2. Choose **Edit > Prepare Scanned Drawings > Peg Bar Alignment...**. The
   Select tool turns on. If there is no selection, the program makes one in
   the middle of the canvas.
3. Move and scale the selection so that it holds the peg hole of every
   Drawing. The selection must hold no other dark mark.
4. In the dialog, select the layers to align in the list **Layer selection**.
5. Click **Align**.

![The Peg bar Alignment dialog](images/animating-pegbar.png)

The dialog stays on top of the window while you work. The line **Reference
Drawing** shows the frame of the reference Drawing. The **Align** button
turns on when a selection exists, the reference Drawing is valid and one layer
is selected.

The program looks for the first dark point in the selection. It uses
the leftmost dark column and the topmost dark row, in the reference
Drawing and in each Drawing of the selected layers. Then it moves each
Drawing by the difference. A pixel is dark when it is solid and its grey
value is below 121. If the program finds no peg hole in a Drawing, it shows
a message and changes nothing. The command is one undo step.
