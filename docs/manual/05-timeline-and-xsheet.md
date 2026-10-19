# Timeline and X-sheet

This chapter explains how to set the timing of an animation. You use the
**Timeline** Panel and the **X-sheet** Panel for this. Both Panels show the
same data: the Drawings of each layer and the frames that they stay on screen.
A change in one Panel shows at once in the other Panel.

To learn where the Timeline is in the main window, read
[Timeline and canvas](01-getting-started.md#getting-started-timeline-canvas).

## Drawings and Exposure {#timeline-drawings-exposure}

A Drawing is one image on a layer. It stays on screen from its first frame
until the next Drawing on that layer starts. The number of frames that a
Drawing stays on screen is its Exposure. A Drawing with an Exposure of two
frames is "on 2s".

![The Timeline with the Rough layer current](images/timeline-and-xsheet-drawings.png)

In the Timeline, each layer has one row. A box shows the first frame of each
Drawing. The colour bar after the box shows the frames that the Drawing holds.
The number ruler is above the rows. The playhead is the blue column. Click the
ruler, or press the Right and Left arrow keys, to move the playhead.

These buttons are in the **Drawings** group of the Timeline. Each button acts on
the current layer:

- **New drawing** (F7) adds a blank Drawing.
- **Remove drawing** (Shift+F5) removes the Drawing at the playhead.
- **Duplicate drawing** (F6) adds a copy of the Drawing at the playhead.

The same commands are in the **Animation** menu. Use the **Zoom** slider in the
Timeline to make the frame boxes wider or narrower.

## Change an Exposure {#timeline-change-exposure}

Use these methods to make an Exposure longer or shorter. Each method moves all
later Drawings of the layer with it, so no Drawing is lost or covered.

- **Exposure +** (the + key) makes the Drawing at the playhead one frame
  longer.
- **Exposure −** (the - key) makes it one frame shorter. An Exposure is never
  shorter than one frame.
- Drag the right edge of the last cell of a Drawing. The edge is the last three
  pixels of the Drawing. When the pointer is on the edge, it changes to a
  two-way arrow. Drag to the right or to the left, and release.

Exposure + and Exposure − are in the **Animation** menu. They are also in the
X-sheet toolbar. Each change is one step in the undo list. Press Ctrl+Z to undo
it.

The last Drawing of a layer has no later Drawing, so it has no edge to drag.
Exposure + and Exposure − do not change it.

![The Drawing at frame 4, before and after two Exposure + commands](images/timeline-and-xsheet-exposure-change.png)

## Insert and delete a frame {#timeline-ripple}

A ripple edit changes one place on a layer and moves all later Drawings of that
layer to make room, or to close the gap.

- **Animation > Insert Frame (Ripple)** (Ctrl+F7) adds a blank Drawing after the
  current frame. It moves every later Drawing of the layer one frame to the
  right.
- **Animation > Delete Frame (Ripple)** (Ctrl+F5) removes the Drawing at the
  playhead, when there is one, and moves every later Drawing of the layer one
  frame to the left. When the playhead is on a hold and not on the first frame
  of a Drawing, the hold becomes one frame shorter.

The commands do nothing on a hidden layer. Delete Frame (Ripple) also does
nothing on the only Drawing of a layer, or after the last Drawing.

![The Rough layer before an Insert Frame (Ripple) and after a Delete Frame (Ripple)](images/timeline-and-xsheet-ripple.png)

## Select Drawings {#timeline-select}

Many commands act on the selected Drawings. To select Drawings:

- Click a cell to select it.
- Drag from one cell to another cell to select a box of cells. The box can go
  over more than one layer.
- Click a cell with Shift held to select all cells from the last selected cell
  to the clicked cell.
- Click a cell with Ctrl held to add or remove that cell.
- Hold Alt and click a cell that is not the first frame of a Drawing, to select
  every Drawing on the layer after that cell.

To clear the selection, click a selected cell. Selected cells are shown darker.

![A box selection on two layers, from frame 4 to frame 10](images/timeline-and-xsheet-select.png)

## Move Drawings {#timeline-move}

To move the selected Drawings, drag one of the selected cells to the left or to
the right. All selected Drawings move the same number of frames, on all layers.
The movement stops at frame 1. When a Drawing is in the way on any layer,
nothing moves.

To move the Drawing at the playhead one frame, use **Animation > Move Drawing
Forward** (Ctrl+.) or **Move Drawing Backward** (Ctrl+,). The Drawing changes
places with a Drawing that is there.

![Three selected Drawings, before and after a move of three frames](images/timeline-and-xsheet-move.png)

## Exposure 1s, 2s, 3s and Clean Exposure {#timeline-exposure-commands}

The commands in **Animation > Timeline Selection** change the Exposure of all
selected Drawings together. When no Drawing is selected, they act on the Drawing
at the playhead.

- **Add Exposure** (Ctrl++) and **Subtract Exposure** (Ctrl+-) change each
  selected Drawing by one frame.
- **Exposure: 1s**, **Exposure: 2s** and **Exposure: 3s** give each selected
  Drawing exactly 1, 2 or 3 frames. Use them to set a whole layer "on 2s" in
  one step.

![A layer before and after Exposure: 2s on all its Drawings](images/timeline-and-xsheet-exposure-set.png)

**Clean Exposure** removes each selected Drawing that has the same image as the
Drawing before it. The Drawing before it then holds over those frames. Use it
after you copy a Drawing to many frames. It works on bitmap and vector layers.
When no Drawing is selected, it cleans the whole current layer.

![Three Drawings with the same image, before and after Clean Exposure](images/timeline-and-xsheet-clean-exposure.png)

## Marks {#timeline-marks}

A Mark is a label on a Drawing that tells its role in the motion. A Drawing has
no Mark, or one Mark:

- **Key** is a main pose of the motion. The Timeline shows a diamond.
- **Breakdown** is the pose that decides how the motion travels between two
  Keys. The Timeline shows a triangle.
- **Inbetween** is a Drawing that fills the motion between Keys and Breakdowns.
  The Timeline shows a dot.

To set a Mark on the Drawing at the playhead, choose **Animation > Mark** and
then the Mark. The keys are Alt+K for Key, Alt+B for Breakdown, Alt+I for
Inbetween and Alt+0 for **Clear Mark**. You can also right-click a Drawing in
the Timeline or in the X-sheet and choose the Mark from the menu. A Mark only
helps you to read the timing. It does not change the picture.

![A Rough layer with Key, Breakdown and Inbetween Marks](images/timeline-and-xsheet-marks.png)

## Markers {#timeline-markers}

A Marker is a named and coloured point on the number ruler of the Timeline. Use
Markers to find important frames, such as the frame where a ball hits the floor.
A Marker is not a Mark: a Mark belongs to a Drawing.

To add a Marker:

1. Move the playhead to the frame.
2. Right-click the number ruler of the Timeline.
3. Choose **Add Marker...**, type a name and click **OK**.

Right-click the ruler again to use these commands: **Rename Marker...**,
**Change Marker Color...**, **Move Marker...** and **Remove Marker**. The
Marker shows as a colour bar under the ruler, with its name next to it when
there is space.

When you right-click a cell of a sound layer, you make a Marker for that layer
only.

![The Timeline with the Markers Start and Hit on the ruler](images/timeline-and-xsheet-markers.png)

## Linked Drawings {#timeline-linked-drawings}

A Linked Drawing shares its image with the Drawing that it was made from. When
you paint on one of them, all of them change. Use a Linked Drawing for a
picture that comes back many times, such as a closed eye in a blink.

To link a Drawing:

1. Hold Alt and press on the first cell of a Drawing.
2. Drag to a frame where no Drawing starts, and release.

A new Drawing is made on that frame. A bar at the bottom of its cell, and of
the cells of the other linked Drawings, shows that they are linked. Linked
Drawings work only on bitmap and vector layers.

![In the X-sheet, the Drawings on frame 1 and frame 11 are linked](images/timeline-and-xsheet-link.png)

To stop the link, right-click a linked Drawing and choose **Unlink Drawing**.
The Drawing keeps its image, but a new paint stroke on it does not change the
other Drawings.

## The X-sheet {#timeline-xsheet}

The **X-sheet** shows the timeline as a table. Each layer is a column. Each
frame is a row. A Drawing shows its number in its first row. A line shows the
rows that it holds. The Mark and the link bar of a Drawing show in its first
cell.

To show the Panel, choose **Window > X-sheet** (Ctrl+5). By default, it is a
tab next to the Timeline.

![The X-sheet Panel with one column for each layer](images/timeline-and-xsheet-xsheet.png)

The X-sheet works like the Timeline, turned to the side:

- Click a cell to move the playhead to that frame and to choose the layer.
- Drag the bottom edge of the last cell of a Drawing to change its Exposure.
- Hold Alt and drag a Drawing to a free row to make a Linked Drawing.
- Right-click a cell for the Mark menu. Right-click a column name for the layer
  menu.

The toolbar of the Panel has the buttons **New drawing**, **Exposure +**,
**Exposure −**, **Insert Frame (Ripple)** and **Delete Frame (Ripple)**. They
work as the commands of the same names. The **Timesheet** button changes the
layout of the columns.

## The Timesheet layout {#timeline-timesheet}

The Timesheet is the layout that Japanese studios use on paper. Click
**Timesheet** in the toolbar of the X-sheet to use it. Click the button again to
go back to one column for each layer.

The Timesheet has fixed columns, in this order: **Key A** to **Key F**,
**Dialogue**, **In-between A** to **In-between F** and **Camera**. A column
that has no layer is empty. The name of the layer is shown next to the name of
its column.

![The X-sheet Panel in the Timesheet layout](images/timeline-and-xsheet-timesheet.png)

JustAnimate puts each layer in a column by these rules:

- A sound layer goes to **Dialogue**.
- A camera layer, or any other layer that holds no bitmap or vector Drawings,
  goes to **Camera**.
- The bitmap and vector layers go to **Key A**, **Key B** and so on, from the
  bottom layer up. After **Key F**, they go to **In-between A**, **In-between B**
  and so on.

To choose the column yourself, right-click the layer in the Timeline, then
choose **Timesheet Column** and the column. The choice **From layer order** goes
back to the rules above. To print a Timesheet on paper, use
**File > Export > Timesheet...**.
