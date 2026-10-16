# JustAnimate

A 2D animation and drawing program. This glossary fixes the words used for its user interface and its drawing concepts.

## Language

### Interface

**Panel**:
A titled region of the main window that holds one group of controls, such as the tools, the colours or the timeline. A Panel is docked in the main window unless the user drags it out; then it is floating.
_Avoid_: Dock, dock widget, palette, window

**Palette**:
A named list of colour swatches that belongs to a project.
_Avoid_: Using "palette" for a Panel

**Layout**:
The arrangement of the Panels in the main window. The default Layout is the one a new installation shows. The user can switch between the named Layouts Draw, Animate and Ink, and reset the current one.
_Avoid_: Workspace, arrangement

**Command bar**:
The single row of buttons under the menu bar for file, edit, view and overlay commands.
_Avoid_: Main toolbar, view toolbar, overlay toolbar

**Tool column**:
The Panel that shows one icon for each tool.
_Avoid_: Toolbox, tool palette

**Tool settings**:
The Panel that shows the adjustable values of the current tool, with size and opacity first. By default it is a one-row strip under the Command bar; docked at a side it shows as a column.
_Avoid_: Tool options, tool property, brush settings

**Accent colour**:
The one colour, chosen by the user from a fixed list, that marks the active tool, selections, slider fill, the playhead and keyboard focus.
_Avoid_: Highlight colour, theme colour

**Welcome screen**:
The screen shown at start when no project is given, with recent projects, new-project templates and Open.
_Avoid_: Start screen, preset dialog, launcher

**Command search**:
The box, opened by a shortcut, that finds a menu command by name and runs it.
_Avoid_: Command palette, quick actions

**Shortcut preset**:
A complete named set of keyboard shortcuts that the user can switch to in one step.
_Avoid_: Keymap, key scheme

**View**:
The pan, zoom, rotation and flip through which the canvas is shown. A change to the View never changes the drawing.
_Avoid_: Camera (a camera is a layer type), viewport

**View tool**:
A tool that changes only the View: hand, zoom and rotate view.
_Avoid_: Navigation tool

**Stroke tool**:
A tool that changes the canvas along the path of the pointer: pencil, pen, brush, eraser and smudge.
_Avoid_: Drawing tool, brush tool (when all five are meant)

**Brush outline**:
The pointer shown over the canvas for a Stroke tool: the outline of the stroke the tool will make, at its true size in the View, with a small cross at its centre.
_Avoid_: Dotted cursor, canvas cursor, tool cursor

### Animation

**Drawing**:
One image on a layer, shown from its first frame until the next Drawing on that layer.
_Avoid_: Key, keyframe, cel, frame (a frame is a moment in time, not an image)

**Exposure**:
The number of frames a Drawing stays on screen. A Drawing "on 2s" has an Exposure of two frames.
_Avoid_: Hold length, duration

**Mark**:
An optional label on a Drawing that tells its role in the motion: Key, Breakdown or Inbetween. A Drawing has at most one Mark.
_Avoid_: Marker (a marker is a named point on the timeline ruler), keyframe type

**Key**:
A Drawing marked as a main pose of the motion.
_Avoid_: Keyframe, using "key" for any Drawing

**Breakdown**:
A Drawing marked as the pose that decides how the motion travels between two Keys.

**Inbetween**:
A Drawing marked as one that fills the motion between Keys and Breakdowns.
_Avoid_: Tween (a tween is made by the program, not drawn)

**Flip**:
Showing Drawings one after another by hand, to check the motion while drawing.
_Avoid_: Scrub (scrubbing moves the playhead along the timeline)

**Rough layer**:
A layer for rough drawings, shown tinted and faded under the other layers and never exported.
_Avoid_: Underlay, sketch layer, reference layer

**Marker**:
A named, coloured point on the timeline ruler.
_Avoid_: Mark (a Mark belongs to a Drawing)

**Linked Drawing**:
A Drawing that shares its image with other Drawings, so painting on one changes all of them.
_Avoid_: Clone, instance, cel reference

**X-sheet**:
A Panel that shows the timeline as a table, one column per layer and one row per frame.
_Avoid_: Exposure sheet, timesheet, dope sheet

**Shift and Trace**:
A mode in which the onion-skinned Drawings around the current one can be moved and rotated in the View, to trace an Inbetween. It never changes a Drawing.
_Avoid_: Out of pegs

**Tool group**:
Tools that share one button in the Tool column; a long press on the button shows the others.
_Avoid_: Tool flyout, tool set
