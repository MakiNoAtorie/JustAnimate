# Brushes and tablet

This chapter explains the settings of the Stroke tools that paint dabs:
Pencil, Brush, Eraser and Smudge. It also explains how to save a set of
settings as a brush preset, and how to set up a graphics tablet.

The settings are in the Tool settings Panel. To learn where the Panels are,
read [The main window](01-getting-started.md#getting-started-main-window).

## Where the brush settings are {#brushes-where}

Choose a Stroke tool in the Tool column, or in the **Tools** menu. The Tool
settings Panel then shows the settings of that tool. The Brush outline on the
canvas shows the true size of the stroke.

![The Tool settings Panel with the Brush tool chosen](images/brushes-and-tablet-tool-settings.png)

A Stroke tool paints a stroke as a row of round or square marks, called dabs.
Each setting in this chapter changes the size, the position, the shape or the
colour of the dabs.

Each tool shows only the settings that it can use. For example, the Eraser has
no Flow and no Opacity. On a vector layer, the
Tool settings Panel shows fewer settings. The dab settings in this chapter
work on bitmap layers.

## Spacing, Flow, Opacity and Wash {#brushes-spacing-flow-opacity-wash}

**Spacing %** is the distance between two dabs, as a percentage of the
Width. A low Spacing gives a smooth line. A high Spacing gives a row of
separate dots. A change to Spacing does not change how dark the stroke is.

![The same stroke with a Spacing of 5, 12, 40, 100 and 250 percent](images/brushes-and-tablet-spacing.png)

**Opacity %** is the most paint that one stroke can put on the canvas. At
50 percent, you can see the layer below through the stroke.

**Flow %** sets how much paint each dab adds. When Flow is low, a stroke is
lighter. When you draw over the same place again, the paint builds up.

![Strokes that cross, with different Opacity and Flow](images/brushes-and-tablet-flow-opacity.png)

**Wash** limits the paint in one stroke. When Wash is on, the parts of a
stroke that overlap are not darker than Opacity. Use Wash for a marker or a
watercolour stroke that crosses itself.

![A stroke that crosses itself, with Wash off and Wash on](images/brushes-and-tablet-wash.png)

The Classic preset keeps the Flow of older versions. See
[Classic preset](#brushes-classic).

## Pressure and the pressure curve {#brushes-pressure-curve}

A tablet pen reports how hard you press. Select **Pressure** in the Tool
settings Panel to use it. The pressure then changes the Width and the
paint of the stroke. When Pressure is off, the stroke has the same width
and paint along its length.

![The same stroke with Pressure off and Pressure on](images/brushes-and-tablet-pressure.png)

The pressure curve decides how the pressure of the pen becomes a size. To
change it:

1. Choose **Edit > Preferences...**.
2. Open the **Tools** page.
3. Look for **Pressure curve (pen pressure to size and opacity)**.
4. Drag a point to move it. Double-click the line to add a point.
   Right-click a point to remove it.

To go back to a straight line, click **Reset Pressure Curve**.

![The pressure curve in the Tools page of the Preferences](images/brushes-and-tablet-pressure-curve.png)

A curve that bends down makes the pen feel harder: you must press more to
get a thick line. A curve that bends up makes the pen feel softer.

The curve is global. A brush preset can have its own curve. See
[Brush presets](#brushes-presets).

## Taper {#brushes-taper}

Taper makes the ends of a stroke thin. **Start taper** gives the stroke a
thin start. **End taper** gives it a thin end. Each value is a length in
pixels: the stroke gets to its full width after that distance.

![A stroke with no taper, a start taper, an end taper and both](images/brushes-and-tablet-taper.png)

Taper also works with a mouse. A mouse has no pressure, but a taper gives
the stroke thin ends, like a pen.

## Speed and tilt {#brushes-dynamics}

Some settings change the size of the dabs while you draw.

- **Speed to size** changes the size with the speed of the pen. A negative
  value makes a fast stroke thinner. A positive value makes it thicker.
- **Tilt to size** changes the size when you tilt the pen. A positive value
  makes a tilted pen paint thicker.
- **Tilt to tip angle** turns the tip in the direction of the tilt.

Each value is from -100 to 100 percent. A value of 0 switches the effect
off. The tilt settings work only with a tablet that reports tilt.

![Speed to size with a slow and a fast stroke, and Tilt to size with an upright and a tilted pen](images/brushes-and-tablet-dynamics.png)

## Tips {#brushes-tips}

The tip is the shape of one dab.

- **Feather** sets how soft the edge of a dab is. At 1, the edge is hard.
  At a high value, the edge fades out.
- **Use Feather** is a check box of the Eraser. It switches the Feather on or
  off.
- **Square Tip** uses a square tip in place of a round tip.
- **Tip Aspect %** sets the height of the tip as a percentage of its width.
  At 100, the tip is round or square. At a low value, the tip is flat, like
  the tip of a chisel marker.
- **Tip Angle** turns the tip counter-clockwise, in degrees. It is useful
  for a flat tip.
- **Follow Direction** turns the tip with the direction of the stroke. The
  Tip Angle then adds to this turn.

To paint with a picture, click **Image Tip...** and choose a PNG file or a
GIMP .gbr brush file. The dark parts of the picture paint. The light parts and
the transparent parts do not paint. The Width sets the size of the picture in
the stroke. Tip Aspect, Tip Angle and Follow Direction also turn and squash
the picture. To go back to the round or square tip, click **Clear Image Tip**.

If the program cannot read the file, it shows a message and keeps the
old tip.

![Round tips with different Feather values, square tips, and an image tip](images/brushes-and-tablet-tips.png)

## Scatter and jitter {#brushes-scatter-jitter}

These settings add random changes to each dab, to make a stroke look
like chalk, charcoal or a spray.

- **Size jitter** makes each dab smaller by a random amount, up to this
  percentage.
- **Scatter** moves each dab away from the line by a random distance, up to
  this percentage of the Width.
- **Angle jitter** turns each dab by a random angle, up to this number of
  degrees.
- **Dabs per step** paints more than one dab at each position. Use it with
  Scatter to make a spray.

All the values are 0 by default. At 0, the setting does nothing.

![Size jitter, Scatter, Dabs per step and Angle jitter](images/brushes-and-tablet-scatter.png)

## Grain {#brushes-grain}

Grain makes a stroke look like it was drawn on paper with a rough surface. The
grain removes paint from the dabs in a pattern.

Use **Grain** to choose how the pattern behaves:

- **Off**: no pattern.
- **Fixed to paper**: the pattern stays on the canvas. A second stroke in the
  same place has the same pattern.
- **Moves with stroke**: the pattern moves with the dabs. Each stroke
  has a streaky look.

Then set the pattern:

- **Grain texture** chooses the picture of the pattern: Generated, Fine
  paper, Crumpled, Laid paper, Creased or Mottled.
- **Grain scale** changes the size of the pattern.
- **Grain contrast** changes how much paint the pattern removes.
- **Grain brightness** makes the pattern lighter or darker.

![Strokes with different Grain settings and textures](images/brushes-and-tablet-grain.png)

## Smoothing {#brushes-smoothing}

Smoothing removes small shakes from your line while you draw. Choose a mode
in the **Smoothing** list and set its strength with the slider next to it.

- **None**: the line follows the pen exactly.
- **1-euro filter**: the program smooths a slow stroke a lot and a fast stroke
  a little. This is the default mode.
- **Weighted**: each point of the line is an average of the points that
  you drew just before it. The line is smooth, and it follows the pen
  closely.
- **Stabilizer**: the line follows the pen at the end of a string. The
  line moves only when the pen moves further than the string length. A
  higher strength makes a longer string. When you lift the pen, the line
  goes to the pen.

![A shaky stroke with each Smoothing mode](images/brushes-and-tablet-smoothing.png)

## Smudge and pigment mixing {#brushes-smudge}

The Smudge tool pushes the paint that is on a bitmap layer along the stroke.
On a vector layer, it moves vertices.

Smudge has these settings, in addition to the dab settings above:

- **Smudge length** sets how long the paint stays on the tool. A high value
  gives a long smear.
- **Smudge sample radius** sets the size of the area under the tool from which
  it takes paint.
- **Smudge colour rate** adds some of the front colour to the smear. At 0,
  the tool uses only the paint that is on the canvas.
- **Smudge mixing** sets how two colours mix:
  - **Light** mixes the colours like light on a screen. Blue and yellow
    make a grey.
  - **Pigment** mixes the colours like real paint. Blue and yellow make
    green.

![Smudging blue and yellow paint with Light mixing and with Pigment mixing](images/brushes-and-tablet-smudge.png)

## Brush presets {#brushes-presets}

A brush preset is a named set of Tool settings. Use a preset to get the same
brush again, in one click.

A preset keeps all the settings of the tool, the image tip, and an optional
pressure curve. A preset is for brushes or for erasers. The Brush Presets
Panel lists only the presets that fit the tool that you use.

### The Brush Presets Panel {#brushes-presets-panel}

Choose **Window > Brush Presets** to show or hide the Brush Presets Panel
(Ctrl+9 by default). Each preset has a sample stroke, a name and a
description.

![The Brush Presets Panel](images/brushes-and-tablet-presets-panel.png)

- Click a preset to use it. If the current tool is not a Stroke tool, the
  program changes to the Brush tool.
- Click **New** to make a preset from the default settings.
- Click **Edit** to change the chosen preset.
- Click **Duplicate** to make a copy of the chosen preset, under a new name.

The program includes presets such as 2B Pencil, HB Pencil, Chalk, G-Pen,
Marker, Soft Airbrush, Watercolour, Hard Eraser and Soft Eraser.

### The preset editor {#brushes-preset-editor}

The preset editor opens when you click **New**, **Edit** or **Duplicate**.
It shows a sample stroke at the top. The sample changes when you change a
value.

![The Brush Preset editor](images/brushes-and-tablet-preset-editor.png)

1. Enter a **Name**. The name must not have path characters such as
   `/` or `:`. You cannot use the name of a different preset.
2. Enter a **Description**. This is optional.
3. To use an image tip, enter the path of a PNG or .gbr file in **Tip
   image**, or click **Browse...**.
4. Set the values. The editor has one row for each setting of the Brush
   tool.
5. To use a pressure curve only for this preset, clear **Use the global
   pressure curve** and edit the curve.
6. Click **Save**.

The program applies the preset that you save.

### Import and export a preset {#brushes-presets-share}

A .jabrush file holds one preset and its image tip. Use it to give a brush
to a different user.

- To import a preset, choose **File > Import > Brush Preset...** and choose
  the .jabrush file. If a preset with the same name exists, the program
  shows a message and does not import the file.
- To export a preset, choose **File > Export > Brush Preset...**, choose
  the preset in the list, and enter a file name.

### The Classic preset {#brushes-classic}

The Classic preset gives the brush of older versions. In older versions,
each dab had its own Flow. When the dabs are close together, the stroke is
darker. The Classic preset has a Flow of 50 and the Pressure setting on.
The preset also switches on a setting that the Tool settings Panel does not
show: Flow per dab.

In all other presets, Spacing does not change how dark a stroke is. In
Classic, a lower Spacing gives a darker stroke.

## Radial tool menu and quick preset picker {#brushes-radial}

The radial menus show a ring of choices under the pointer. They are fast
when you draw with a pen, because you do not need to move to a Panel.

To pick a tool:

1. Put the pointer on the canvas.
2. Press **Q**, or choose **Tools > Radial Tool Menu**. A ring of tools
   opens around the pointer.
3. Move the pointer over a tool. Tap or release the pen to choose it.

![The radial tool menu](images/brushes-and-tablet-radial-menu.png)

To pick a brush preset:

1. Choose a Stroke tool.
2. Put the pointer on the canvas.
3. Press **W**, or choose **Tools > Quick Preset Picker**. A ring of
   presets opens around the pointer.
4. Move the pointer over a preset. Tap or release the pen to apply it to
   the tool.

![The quick preset picker](images/brushes-and-tablet-preset-picker.png)

The ring shows the first 16 presets of the tool. You can change the keys in
**Edit > Preferences... > Shortcuts**.

## Tablet setup {#brushes-tablet}

JustAnimate reads the pressure, the tilt and the position of a graphics tablet
pen. To set up a tablet:

1. Install the driver of the tablet. Restart the computer if the driver
   asks you to.
2. Start JustAnimate and choose a Stroke tool, such as the Brush.
3. Make sure that **Pressure** is on in the Tool settings Panel.
4. Draw a stroke and press the pen harder and softer. The width of the stroke
   must change.
5. To change the response of the pen, edit the
   [pressure curve](#brushes-pressure-curve).

If the pressure does not work, change the input system in **Edit >
Preferences... > General**:

- **Tablet input API** is for Windows only. The choices are **WinTab** and
  **Windows Ink**. WinTab needs a WinTab driver. If the program cannot use
  WinTab, the list shows Windows Ink. If pen pressure does not work with one
  choice, try the other.
- **Tablet high-resolution position** uses the sub-pixel position that the
  tablet reports. It gives smoother lines.

![The tablet settings in the General page of the Preferences](images/brushes-and-tablet-tablet.png)

On macOS and Linux, there is no choice of input system. The system driver of
the tablet is used.
