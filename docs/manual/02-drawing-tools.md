# Drawing tools {#drawing-tools}

This chapter describes the tools that you use to draw, fill, select and move
parts of a Drawing, and the tools that change the View. It is in the order
that you meet the tools in the Tool column.

## The Tool column {#drawing-tools-tool-column}

The Tool column is the Panel that shows one icon for each tool. The chapter
[Getting started](01-getting-started.md#getting-started-tool-column) shows
where it is in the main window. Click an icon
to select the tool. The icon of the current tool uses the Accent colour.

![The Tool column with the Pencil selected](images/drawing-tools-tool-column.png)

The same tools are in the Tools menu. Each tool has a shortcut. Put the
pointer on an icon to see its name and shortcut.

| Tool | Shortcut | Tool | Shortcut |
|---|---|---|---|
| Pencil | N | Lasso Fill | L |
| Pen | P | Eyedropper | I |
| Brush | B | Move | M |
| Eraser | E | Select | V |
| Smudge | A | Hand | H |
| Polyline | Y | Zoom | Z |
| Shape | U | Rotate View | R |
| Bucket | K | | |

You can change each shortcut in the Preferences. The status bar at the bottom
of the main window shows a short hint for the current tool.

The Tool settings Panel shows the adjustable values of the current tool. Width
and opacity are first. The Panel has no values for the Lasso Fill and the
Eyedropper.

### Tool groups {#drawing-tools-groups}

Some tools share one button. A set of tools with one button is a Tool group.
The button shows the tool that you used last in the group.

![The Tool group menu of the Pencil button](images/drawing-tools-tool-group.png)

To select another tool of a group, do the steps that follow.

1. Press and hold the button, or right-click it. A menu shows the tools of the
   group.
2. Select a tool in the menu.

The button then shows this tool. The program keeps this choice for the next
start. A click on the button selects the tool that it shows. The shortcut of
each tool always selects that tool.

The Tool groups are:

- Pencil, Pen and Brush
- Eraser and Smudge
- Move and Select
- Bucket and Lasso Fill
- Hand, Zoom and Rotate View

Polyline, Shape and Eyedropper have their own buttons.

## The Stroke tools {#drawing-tools-stroke-tools}

The Pencil, Pen, Brush, Eraser and Smudge are Stroke tools. A Stroke tool
changes the canvas along the path of the pointer. All five tools work on
bitmap layers and on vector layers.

These keys work with the Pencil, Pen and Brush:

- Hold Alt to pick a colour from the canvas. When you release Alt, the program
  goes back to your tool. See [Eyedropper](#drawing-tools-eyedropper).
- Hold Ctrl and Shift to erase with the tool.
- Hold Space to move the canvas with the Hand tool.

To change the size of a tool without the Tool settings Panel, hold Shift and
drag. The Brush outline grows or shrinks. When you release the pointer, the
tool keeps the new Width. The Brush, Eraser and Smudge also change their
Feather when you hold Ctrl and drag.

Each Stroke tool has more values in the Tool settings Panel. These values are
Smoothing, Pressure, Spacing, tip shape and grain. The chapter
[Brushes and tablet](03-brushes-and-tablet.md#brushes-where) describes them.

### Pencil {#drawing-tools-pencil}

The Pencil draws a line like a pencil on paper. With a pen tablet, the
pressure of the pen changes the line.

![A pencil drawing with the Pencil](images/drawing-tools-pencil.png)

To draw, drag the pointer on the canvas. The Pencil is the right tool for
rough drawings and for first sketches.

### Pen {#drawing-tools-pen}

The Pen draws a clean line of ink. [Smoothing](03-brushes-and-tablet.md#brushes-smoothing) makes the line less shaky.

![Two lines of ink drawn with the Pen](images/drawing-tools-pen.png)

The Pen has these values in the Tool settings Panel: Width, Smoothing,
Pressure and Anti-Aliasing. Use the Pen for the final line of a Drawing.

### Brush {#drawing-tools-brush}

The Brush paints a soft stroke with the current colour. The Feather value
makes the edge of the stroke soft or hard. You can load a Brush preset from
the [Brush Presets Panel](03-brushes-and-tablet.md#brushes-presets-panel).

![Two soft strokes painted with the Brush](images/drawing-tools-brush.png)

### Eraser {#drawing-tools-eraser}

The Eraser removes the lines that you drew. On a bitmap layer, it removes the
pixels that it touches. On a vector layer, it removes strokes.

![The Eraser removes a part of two lines](images/drawing-tools-eraser.png)

Use Width and Feather in the Tool settings Panel to set the size and the edge
of the Eraser.

### Smudge {#drawing-tools-smudge}

The Smudge pulls the colour along the path of the pointer. On a bitmap layer,
it smears the pixels. On a vector layer, it changes the shape of a line. The
settings of the pixel Smudge are in
[Smudge and pigment mixing](03-brushes-and-tablet.md#brushes-smudge).

![The Smudge pulls the colour of two strokes down](images/drawing-tools-smudge.png)

## The Polyline tool {#drawing-tools-polyline}

The Polyline tool draws a line from straight parts. Each click adds a point.

![A polyline that is not complete](images/drawing-tools-polyline.png)

1. Click on the canvas to set the first point.
2. Click to add more points. The line follows the pointer.
3. To finish the line, double-click or press Enter.

During the line, these keys are available:

- Backspace removes the last point.
- Escape discards the line.

The Tool settings Panel has these values: Width, Closed Path, Bézier and
Anti-Aliasing. If you select Closed Path, the program joins the last point to
the first point. Hold Ctrl to switch Closed Path for the line that you draw. If
you select Bézier, the line is a smooth curve through the points.

## The Shape tool {#drawing-tools-shape}

The Shape tool draws a line, a rectangle, an ellipse or a polygon. The shape
has a line and no fill.

![A rectangle, an ellipse, a line and a polygon drawn with the Shape tool](images/drawing-tools-shape.png)

Use the Shape list in the Tool settings Panel to select the kind of shape.

To draw a line, a rectangle or an ellipse, drag from one corner to the other.
These keys change the shape while you drag:

- Hold Shift to constrain the shape. A line goes in steps of 45 degrees. A
  rectangle becomes a square. An ellipse becomes a circle.
- Hold Alt to draw from the centre.

To draw a polygon, click to add each point. To close the polygon, double-click
or click on the first point. Press Escape to discard a polygon that is not
complete.

## The Bucket tool {#drawing-tools-bucket}

The Bucket fills an area with the current colour. Click inside an area that a
line closes.

![Two shapes that the Bucket filled with colour](images/drawing-tools-bucket.png)

If you drag the pointer across more areas, the Bucket fills each area. One
Undo step reverses all the fills of one drag. Hold Alt to pick a colour from
the canvas first.

The Tool settings Panel has these values for a bitmap layer:

- **Reference** sets the layers that the Bucket reads to find the area. Select
  Current layer or All layers.
- **Blend mode** sets how the fill goes on the Drawing. Select Overlay,
  Replace or Behind. Behind puts the colour behind the lines, so the lines
  stay on top. Use Behind to colour a drawn outline.
- **Color tolerance** fills colours that are almost the same as the colour that
  you click. Select the check box to turn it on. It is off by default.
- **Expand fill** grows the filled area by a number of pixels. It is on by
  default.
- **Close gaps** treats a gap in the lines as closed when the gap is not wider
  than this number of pixels. The value 0 turns it off.

Use Close gaps when a line of the outline has a small opening. Without it, the
fill goes out through the opening and fills the area around the outline.

On a vector layer, the Bucket paints the stroke or the area that you select.
The Stroke thickness value sets the width of the stroke.

## The Lasso Fill tool {#drawing-tools-lasso-fill}

The Lasso Fill fills an area that you draw by hand with the current colour.

![A freehand area that the Lasso Fill filled](images/drawing-tools-lasso-fill.png)

1. Drag the pointer around the area.
2. Release the pointer. The program joins the end of the loop to the start of
   the loop and fills the loop.

Use the Lasso Fill for an area that no line closes. It works on bitmap layers
and on vector layers. On a bitmap layer with Lock Transparency on, it paints
only where the Drawing has pixels.

## The Eyedropper tool {#drawing-tools-eyedropper}

The Eyedropper picks a colour from the canvas. The colour becomes the current
colour.

![The Eyedropper picks the red of a stroke](images/drawing-tools-eyedropper.png)

Click on the canvas. On a bitmap layer, the Eyedropper reads the pixel in the
current Drawing. On a vector layer, it reads the colour of the stroke or the
fill near the pointer. The Eyedropper does not pick from an empty place.

You do not have to select the Eyedropper. Hold Alt while you use a Stroke tool
or the Bucket. The tool changes to the Eyedropper. When you
release Alt, the program goes back to the tool that you used before.

## The Select and Move tools {#drawing-tools-select-move}

The Select tool and the Move tool share one button. The Select tool makes a
selection. The Move tool changes the selected part.

### Select {#drawing-tools-select}

Drag on the canvas to draw a rectangle around the part to select. The
selection shows as a dashed line with handles.

![A selection around a drawn ellipse](images/drawing-tools-select.png)

To change the selection, drag one of its handles. Hold Alt to change the
content of the selection. Press Backspace to clear the content of the
selection.

### Move {#drawing-tools-move}

The Move tool changes the selection with the handles on its edge.

![The Move tool shows the handles of a selection](images/drawing-tools-move.png)

- Drag inside the selection to move it.
- Hold Ctrl and drag to rotate it.
- Drag a square handle at a corner to scale it. The scale keeps the aspect
  ratio. Hold Shift to scale without this limit.
- Drag a round handle at the middle of an edge to skew the selection about its
  centre.

On a vector layer, click on or near a stroke to select it, then drag.

You can also type values. The Tool settings Panel has fields for the Position,
the Scale and the Rotation of the selection. Position is the centre of the
selection. Type a value and press Enter to apply it. The Anti-Aliasing check
box smooths the edges when the Move tool changes pixels. The Show Size and
Diff. check box shows size information on the canvas.

### Transform Key {#drawing-tools-transform-key}

The Transform Key check box is in the Tool settings Panel of the Move tool.
When it is on, the Move tool does not change the pixels. It changes the whole
current Drawing as one unit.

![The Move tool with Transform Key on moves the whole Drawing](images/drawing-tools-transform-key.png)

- Drag to move the Drawing.
- Hold Ctrl and drag to rotate it.
- Hold Shift and drag to scale it.

The Tool settings Panel shows the values of the Drawing: Move X, Move Y,
Rotation, Scale and Opacity. Type a value in a field and press Enter to set
it. The tween eases the motion between the key transforms of two Drawings.

## The View tools {#drawing-tools-view-tools}

A View tool changes only the View. It never changes a Drawing. The Hand, Zoom
and Rotate View tools are View tools.

### Hand {#drawing-tools-hand}

The Hand moves the canvas in the window.

![The canvas after a drag with the Hand](images/drawing-tools-hand.png)

- Drag to pan.
- Hold Ctrl and drag to zoom, or drag with the right button.
- Hold Alt and drag to rotate the View.
- Double-click with the right button to reset the View.

When you use a Stroke tool, hold Space to use the Hand for a short time.

### Zoom {#drawing-tools-zoom}

Click with the Zoom tool to zoom in at the pointer. Hold Alt and click to
zoom out. Drag to the right to zoom in. Drag to the left to zoom out.

![The canvas after one click with the Zoom tool](images/drawing-tools-zoom.png)

The status bar and the zoom box at the bottom of the window show the zoom
level. The View menu has Zoom In, Zoom Out and a Zoom submenu with set levels.
The keys 1, 2, 3 and 4 set the zoom to 100%, 200%, 300% and 400%.

### Rotate View {#drawing-tools-rotate-view}

Drag with the Rotate View tool to turn the canvas about its centre. This is
the same as turning a sheet of paper on a desk.

![The canvas turned by 25 degrees](images/drawing-tools-rotate-view.png)

The Tool settings Panel has a Reset View button. It sets the rotation back to
0 degrees. The View menu has the same commands: Rotate Clockwise, Rotate
Anticlockwise and Reset Rotation. The View menu command Reset sets the zoom
and the rotation. Center View puts the canvas at the centre of the window.

## The Brush outline {#drawing-tools-brush-outline}

Each Stroke tool shows the Brush outline over the canvas. The Brush outline is
the outline of the stroke that the tool will make. It has the true size of the
stroke in the View, with a small cross at its centre. It follows the zoom and
the rotation of the View.

![The Brush outline of a large Brush](images/drawing-tools-brush-outline.png)

If the Feather is on, a second circle shows the soft edge. When the outline is
smaller than 6 pixels, the program shows only the cross.

To show only the cross for all tools, do the steps that follow.

1. Select Edit > Preferences....
2. Open the General page.
3. In the Appearance group, set Cursor to Cross only.

Set Cursor to Brush outline to show the outline again.

## The Ruler and Snap to Perspective {#drawing-tools-ruler}

Two commands in the View > Overlays menu help you to draw straight lines.

![The View > Overlays menu](images/drawing-tools-overlays-menu.png)

### Ruler {#drawing-tools-ruler-line}

Select View > Overlays > Ruler to show a straight line on the canvas. A
stroke that starts near the ruler follows it.

![A stroke that follows the ruler](images/drawing-tools-ruler.png)

To move the ruler, select the Move tool. Drag an end point of the ruler to a
new place. Clear the Ruler item to hide the ruler.

### Snap to Perspective {#drawing-tools-snap-perspective}

First select a perspective overlay in the View > Overlays menu. The choices
are One Point Perspective, Two Point Perspective and Three Point Perspective.
Then select View > Overlays > Snap to Perspective.

![Two strokes that follow the lines to a vanishing point](images/drawing-tools-perspective.png)

Each stroke now follows the line from its start point to a vanishing point of
the overlay. Draw a stroke toward the vanishing point to make a straight line
in the perspective. Clear the item to draw freely again.

## Navigation with the mouse wheel {#drawing-tools-wheel}

The wheel of the mouse moves the View over the canvas. It does not change a
Drawing. The wheel does not work while a stroke is in progress.

![The canvas after Ctrl and the wheel zoom in](images/drawing-tools-wheel.png)

| Action | Result |
|---|---|
| Wheel | Pans up and down |
| Shift and wheel | Pans to the left and to the right |
| Ctrl and wheel | Zooms at the pointer |
| Alt and wheel | Rotates the View about the pointer, 15 degrees for each step |

The General page of the Preferences has a Navigation group. Its check box
Invert wheel zoom turns the direction of the zoom around.

To read about the other parts of the program, go back to the
[start page](00-index.md#manual-start).
