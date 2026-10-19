# Preferences and reference {#preferences}

This chapter describes every page of the Preferences window and its default
values. It also has a table of the default keyboard shortcuts and a glossary of
the words that this manual uses.

## Open the Preferences {#prefs-open}

Choose **Edit > Preferences...** or press Ctrl+K. The **Preferences** window
opens.

![The Edit menu, with Preferences...](images/preferences-and-reference-edit-menu.png)

The window has a **Search preferences** box at the top. At the left, a list
shows the five pages: **General**, **Files**, **Timeline**, **Tools** and
**Shortcuts**. Click a page to open it.

There is no OK button or Apply button. Each change takes effect at once and the
program keeps it for the next start. Click **Close** to close the window. Only
one Preferences window is open at a time. If you choose Preferences... again,
the program brings the open window to the front.

The values in this chapter are the defaults of a new installation. A default is
the value that the program uses until you change it.

## The General page {#prefs-general}

The General page has the settings for the language, the look of the program, the
canvas, the tablet, the grid, navigation, memory and undo. The page is long.
Use the scroll bar or the mouse wheel to reach the lower groups.

![The top of the General page](images/preferences-and-reference-general-top.png)

### Language, Theme and Accent colour {#prefs-general-look}

The first group, **Language**, has three lists.

- The language list sets the language of the menus and dialogs. The default is
  **[System-Language]**, which uses the language of your operating system. When
  you choose a language, the message **Restart Required** shows. The change
  takes effect after you restart JustAnimate.
- The **Theme** list has **System**, **Light** and **Dark**. The default is
  **System**.
- The **Accent** list has eight colours. The default is **Blue**.

The Theme and the Accent colour change the program at once. For more
information, see [Theme and Accent colour](01-getting-started.md#getting-started-theme-accent).

### Window opacity and Appearance {#prefs-general-appearance}

- **Opacity** is a slider from 30 to 100. At 100 the main window is solid. A
  lower value makes the window more see-through. The default is 100.
- **Shadows** draws shadows under the Panels. It is off by default.
- **Cursor** sets the pointer over the canvas. **Brush outline** shows the
  outline of the stroke, with a small cross at its centre. **Cross only** shows
  only the cross. The default is **Brush outline**. For more information, see
  [The Brush outline](02-drawing-tools.md#drawing-tools-brush-outline).

### Background and Canvas {#prefs-general-canvas}

The **Background** group has five round buttons. Each button shows a small
picture of the background that it selects: checkerboard, white, grey, dots and
weave. The default is the white background. The background is only for your
screen. It is not part of a Drawing and it is not exported.

The **Canvas** group has two boxes.

- **Antialiasing** smooths the edges of lines on the screen. It is on by default.
- **Fast rendering of layers above** keeps the layers above the layer that you
  paint in a cache. This makes repaints faster when there are many layers. It is
  off by default.

### Editing and the tablet {#prefs-general-editing}

![The middle of the General page](images/preferences-and-reference-general-middle.png)

- **Vector curve smoothing** is a slider from 1 to 100. It sets how much the
  program smooths a vector curve after you draw it. The default is 20.
- **Tablet high-resolution position** uses the sub-pixel position that a
  graphics tablet reports. It is on by default.
- **Tablet input API** is on Windows only. The choices are **WinTab** and
  **Windows Ink**. The default is WinTab. If there is no WinTab driver, the
  list shows Windows Ink and the program uses Windows Ink.

For more information on tablets, see
[Tablet setup](03-brushes-and-tablet.md#brushes-tablet).

### Grid and Overlays {#prefs-general-grid}

- **Enable Grid** shows a grid over the canvas. It is off by default.
- **Grid Width** and **Grid Height** set the distance between the grid lines, in
  pixels. The range is 1 to 9999. The default is 100 for each.
- **Enable Action Safe area (%)** shows the area where all important action must
  stay. Set its size in the box next to it, from 1 to 25. It is on by default,
  at 5.
- **Enable Title Safe area (%)** shows the area where all titles must stay. The
  range is 1 to 25. It is on by default, at 10.
- **Show Safe area labels** writes a label on each safe-area line. It is on by
  default. It is not available when both safe areas are off.

These settings only prepare the Overlays. To show or hide an Overlay, use its
button in the Command bar or its command in the **View > Overlays** menu.

### Navigation, Advanced and Undo/Redo {#prefs-general-navigation}

![The bottom of the General page](images/preferences-and-reference-general-bottom.png)

- **Invert wheel zoom** reverses the direction of the zoom for the mouse wheel.
  It is off by default.
- **Invert hand-tool zoom drag** zooms in when you drag the Hand tool up, not
  down. It is off by default. For more information, see
  [Navigation with the mouse wheel](02-drawing-tools.md#drawing-tools-wheel).
- **Memory Cache Budget** is the largest amount of memory that the program uses
  to keep drawn frames ready. The range is 100 MB to 16000 MB. The default is
  1024 MB. A larger value is faster but uses more RAM.
- **Maximum Number of Undo/Redo Steps** is the number of steps that you can undo
  and redo. The range is 20 to 250. The default is 100. When you change this
  value, the program shows the warning **Resets your current undo history**.
  Click **Yes** to keep the new value. Click **No** to keep the old value. See
  [Undo and redo](09-saving-and-safety.md#saving-undo).

## The Files page {#prefs-files}

The Files page has the settings for the start of the program and for autosave.

![The Files page](images/preferences-and-reference-files.png)

### Startup Settings {#prefs-files-startup}

The list at the top shows the presets. A preset is a project that you can start
a new project from. The first preset, **Blank**, is always there. The default
preset is shown in bold. At first it is **Blank**.

- Click **+** to save the current project as a new preset. The new name is
  editable at once. The program removes the characters `@`, `/` and `\` from a name.
- Click **-** to remove the selected preset. You cannot remove **Blank**. If you
  remove the default preset, **Blank** is the default again.
- Click **Make Default** to make the selected preset the default preset.

The three round buttons below the list set what the program does at start.

- **Show the Welcome screen** opens the Welcome screen. This is the default.
- **Open the last project** opens the project that you used last.
- **New project from the default preset** starts a new project at once.

For more information, see
[The Welcome screen](01-getting-started.md#getting-started-welcome-screen).

### Autosave documents {#prefs-files-autosave}

- **Enable autosave by number of modifications** saves the project to its file
  after a number of changes. Set the number in **Number of modifications before
  autosaving**, from 4 to 8192. The default is off, with 256.
- **Remind me to save an untitled project** shows a notice when the project has
  no file yet and the count autosave cannot save it. It is on by default.
- **Enable autosave by time** writes a backup after a period of time. Set the
  period in **Autosave period (minutes)**, from 1 to 300. The default is on,
  with 5 minutes.

For more information, see [Autosave](09-saving-and-safety.md#saving-autosave).

## The Timeline page {#prefs-timeline}

The Timeline page has the settings for the Timeline, drawing on an empty frame,
Flip, sound and layer visibility. The page is long. Scroll to reach the lower
groups.

![The top of the Timeline page](images/preferences-and-reference-timeline.png)

### Timeline {#prefs-timeline-group}

- **Timeline length** is the number of frames that the Timeline shows. The range
  is 1 to 9999. The default is 240.
- **Short scrub** changes the scrub mark in the Timeline. When it is on, the mark
  is short and covers only the frame ruler. When it is off, the mark runs down
  through all layers. It is off by default. You can also switch it when you
  double-click the frame ruler. It does not change the sound. To set the
  length of the sound, use [Sound scrub](#prefs-timeline-sound).

### Drawing {#prefs-timeline-drawing}

The group **Drawing** sets what happens when you draw on an empty frame. It
applies to the Pencil, Eraser, Pen, Polyline, Bucket and Brush tools.

- **Create a new (blank) Drawing** starts a new Drawing on the frame. This is
  the default.
- **Duplicate the previous Drawing** copies the previous Drawing to the frame
  and starts to draw on the copy.
- **Keep drawing on the previous Drawing** draws on the previous Drawing, so the
  empty frame stays empty.

### Flip and Roll {#prefs-timeline-flip}

![The bottom of the Timeline page](images/preferences-and-reference-timeline-bottom.png)

- **Maximum numbers of drawings in roll** is how many Drawings a Flip Rolling
  shows at once. The range is 3 to 10. The default is 5.
- **Msecs per drawing in flip roll** is the time that each Drawing shows in a
  Flip Rolling. The range is 100 to 400 ms. The default is 100 ms.
- **Msecs per drawing in flip inbetween** is the time that each Drawing shows in
  a Flip In-Between. The range is 100 to 400 ms. The default is 100 ms.

For more information, see
[Flip In-Between and Flip Rolling](06-animating.md#animating-flip-roll).

### Sound scrub {#prefs-timeline-sound}

The slider and the box set the length of the sound that plays at each frame
when you scrub. The value is in milliseconds. The default is 100 ms. The
smallest value is the length of one frame, or 100 ms if a frame is longer. The
largest value is 200 ms. See [Scrubbing with sound](07-sound-and-lip-sync.md#sound-scrub).

### Layer Visibility {#prefs-timeline-visibility}

- **Startup option** sets which layers are visible when you open a project:
  **Current layer only**, **Relative** or **All Layers**. The default is **All
  Layers**.
- The slider **When layer visibility is relative (gray dot)** sets how visible
  the other layers are in the **Relative** mode. It is available only when the
  startup option is **Relative**. The default is 50%.

## The Tools page {#prefs-tools}

The Tools page has the settings for the Stroke tools and the Move tool.

![The Tools page](images/preferences-and-reference-tools.png)

- **Use Quick Sizing** lets you change the brush size and the feather with a key
  and a drag on the canvas. It is on by default.
- The **Pressure curve** graph sets how the pen pressure changes the width and
  the opacity of a stroke. The default is a straight line. Drag the points to
  change it. For more information, see
  [Pressure and the pressure curve](03-brushes-and-tablet.md#brushes-pressure-curve).
- **Reset Pressure Curve** sets the curve back to a straight line.
- **Reset all tools to default** sets every tool setting back to its default
  value. The program first asks **Reset every tool setting to its default?**
  Click **Yes** to continue.
- **Rotation snap increment** is the angle step for a rotation when you hold
  Shift. The slider shows only angles that divide 360 without a remainder,
  such as 1, 5, 10, 15, 30 and 45 degrees. The default is 15 degrees. The Move
  tool and the camera tool use this value.

## Preference search {#prefs-search}

The **Search preferences** box at the top of the window finds a setting. Type one
or more words. A word can be a part of a name, and the case does not matter.

![A search for "grid" in the General page](images/preferences-and-reference-search.png)

- The program looks at the titles of groups, the labels and the text of boxes
  and buttons. It does not look at the items inside a list or at tooltips.
- When there are more words, a name must have all of them.
- Each match gets a coloured background. The program opens the first page that
  has a match.
- On the Shortcuts page, the search hides the commands whose names do not match.
- To stop the search, clear the box. The backgrounds go and the Shortcuts page
  shows all commands again.

## The Shortcuts page {#prefs-shortcuts}

The Shortcuts page lets you change the keyboard shortcut of each command.

![The Shortcuts page](images/preferences-and-reference-shortcuts.png)

The table lists the commands in the column **Action**, with their keys in the
column **Shortcut**. To change a key:

1. Click a command in the table. Its name shows next to **Action:**.
2. Click the box next to **Shortcuts:**.
3. Press the new key or key combination.

The program saves the new key at once. Click **Clear** to remove the key of the
selected command. A command can be without a key.

The three buttons below the table work on all commands.

- **Save** writes all shortcuts to a file of the type .pcls.
- **Load** reads the shortcuts of a .pcls file. A key in the file replaces the
  key of the same command.
- **Restore Default Shortcuts** sets all shortcuts back to their default keys.
  It does not ask first.

### Shortcut conflicts {#prefs-conflict}

Two commands cannot use the same key. If you press a key that another command
already uses, a warning shows.

![The warning for a shortcut conflict](images/preferences-and-reference-conflict.png)

The warning says **Shortcut Conflict!** and names the other command. The default
answer is **No**.

- Click **No** to keep the old key. The box goes back to the old key and nothing
  changes.
- Click **Yes** to give the key to the selected command. The other command then
  has no key.

The check is made when you press a key in the box. **Load** does not check for
conflicts.

## Default keyboard shortcuts {#reference-shortcuts}

These are the default shortcuts of a new installation. The program shows them
next to the commands in the menus.

![The Layer menu, with the shortcuts next to the commands](images/preferences-and-reference-layer-menu.png)

On macOS the program shows the Ctrl key as the Command key. The names of the
commands are the names that the Shortcuts page shows. The Shortcuts page also
lists commands that have no default key. They are not in these tables.

### File {#reference-shortcuts-file}

| Command | Key |
|---|---|
| New File | Ctrl+N |
| Open File | Ctrl+O |
| Save File | Ctrl+S |
| Save File As | Ctrl+Shift+S |
| Export Image Sequence | Ctrl+R |
| Export Image | Ctrl+Shift+R |
| Export Movie | Ctrl+M |
| Export Animated GIF | Ctrl+G |
| Exit | Ctrl+Q |

### Edit {#reference-shortcuts-edit}

| Command | Key |
|---|---|
| Undo | Ctrl+Z |
| Redo | Ctrl+Shift+Z |
| Cut | Ctrl+X |
| Copy | Ctrl+C |
| Paste | Ctrl+V |
| Paste from previous drawing | Ctrl+Left |
| Select All | Ctrl+A |
| Deselect All | Ctrl+D |
| Command search | Ctrl+Shift+P |
| Preferences | Ctrl+K |

### View {#reference-shortcuts-view}

| Command | Key |
|---|---|
| Zoom In | Ctrl+Up |
| Zoom Out | Ctrl+Down |
| Set Zoom to 400% | 4 |
| Set Zoom to 300% | 3 |
| Set Zoom to 200% | 2 |
| Set Zoom to 100% | 1 |
| Set Zoom to 50% | Shift+2 |
| Set Zoom to 33% | Shift+3 |
| Set Zoom to 25% | Shift+4 |
| Reset Rotation | Alt+H |
| Reset View | Ctrl+H |
| Center View | Ctrl+Shift+H |
| Mirror View Horizontally | Shift+H |
| Mirror View Vertically | Shift+V |
| Toggle Grid | G |
| Toggle Onion Skin | O |
| Toggle Shift and Trace | Shift+T |

### Playback and Flip {#reference-shortcuts-playback}

| Command | Key |
|---|---|
| Play/Stop | Ctrl+Return |
| Play Around Playhead | Alt+Return |
| Toggle Loop | Ctrl+L |
| Flip In-Between | Alt+Z |
| Flip Rolling | Alt+X |
| Flip (hold and drag the pen) | F |
| Next Frame | . |
| Previous Frame | , |
| Next drawing | Alt+. |
| Previous drawing | Alt+, |

### Drawings, Exposure and Marks {#reference-shortcuts-drawings}

| Command | Key |
|---|---|
| New drawing | F7 |
| Duplicate drawing | F6 |
| Remove drawing | Shift+F5 |
| Insert Frame (Ripple) | Ctrl+F7 |
| Delete Frame (Ripple) | Ctrl+F5 |
| Move Drawing Forward | Ctrl+. |
| Move Drawing Backward | Ctrl+, |
| Exposure + | + |
| Exposure - | - |
| Selection: Add Exposure | Ctrl++ |
| Selection: Subtract Frame Exposure | Ctrl+- |
| Mark: Key | Alt+K |
| Mark: Breakdown | Alt+B |
| Mark: Inbetween | Alt+I |
| Clear Mark | Alt+0 |
| Create Inbetweens | Ctrl+Shift+I |

### Tools {#reference-shortcuts-tools}

| Command | Key |
|---|---|
| Move Tool | M |
| Select Tool | V |
| Brush Tool | B |
| Polyline Tool | Y |
| Shape Tool | U |
| Lasso Fill Tool | L |
| Smudge Tool | A |
| Pen Tool | P |
| Hand Tool | H |
| Rotate View Tool | R |
| Zoom Tool | Z |
| Pencil Tool | N |
| Bucket Tool | K |
| Eyedropper Tool | I |
| Eraser Tool | E |
| Radial Tool Menu | Q |
| Quick Preset Picker | W |

For the momentary Alt key of the Eyedropper, see
[The Eyedropper tool](02-drawing-tools.md#drawing-tools-eyedropper).

### Layers {#reference-shortcuts-layers}

| Command | Key |
|---|---|
| New Bitmap Layer | Ctrl+Alt+B |
| New Vector Layer | Ctrl+Alt+V |
| New Sound Layer | Ctrl+Alt+W |
| New Camera Layer | Ctrl+Alt+C |
| Show Current Layer Only | Alt+1 |
| Show Layers Relative to Current Layer | Alt+2 |
| Show All Layers | Alt+3 |
| Rename Layer | F2 |

### Panels {#reference-shortcuts-panels}

The Shortcuts page calls a Panel a "Window". Each key shows or hides one Panel.

| Command | Key |
|---|---|
| Toggle Tools Window Visibility | Ctrl+1 |
| Toggle Options Window Visibility | Ctrl+2 |
| Toggle Color Box Window Visibility | Ctrl+3 |
| Toggle Color Palette Window Visibility | Ctrl+4 |
| Toggle X-sheet Window Visibility | Ctrl+5 |
| Toggle Timeline Window Visibility | Ctrl+6 |
| Toggle Color Inspector Window Visibility | Ctrl+7 |
| Toggle Onion Skins Window Visibility | Ctrl+8 |
| Toggle Brush Presets Window Visibility | Ctrl+9 |
| Toggle Storyboard Window Visibility | Ctrl+0 |

## Glossary {#reference-glossary}

These are the words that the program and this manual use for its interface and
its drawing concepts. In the picture, the Brush Presets, Tool settings, Color
Box, Palette and Timeline are Panels. Together they make one Layout.

![The main window: its Panels make a Layout](images/preferences-and-reference-main-window.png)

- **Accent colour.** The colour that marks the active tool, selections, the
  slider fill, the playhead and the keyboard focus. You choose it from a fixed
  list. See [Theme and Accent colour](01-getting-started.md#getting-started-theme-accent).
- **Breakdown.** A Drawing that has the Mark Breakdown. It is the pose that
  decides how the motion goes between two Keys. See
  [Marks](05-timeline-and-xsheet.md#timeline-marks).
- **Brush outline.** The pointer over the canvas for a Stroke tool: the outline
  of the stroke that the tool makes, with a small cross at its centre. See
  [The Brush outline](02-drawing-tools.md#drawing-tools-brush-outline).
- **Command bar.** The row of buttons under the menu bar for file, edit, view
  and overlay commands. See [Command bar](01-getting-started.md#getting-started-command-bar).
- **Command search.** The box that finds a menu command by name and runs it. See
  [Command search](01-getting-started.md#getting-started-command-search).
- **Drawing.** One image on a layer. It shows from its first frame until the
  next Drawing on that layer. A frame is a moment in time, not an image. See
  [Drawings and Exposure](05-timeline-and-xsheet.md#timeline-drawings-exposure).
- **Exposure.** The number of frames that a Drawing stays on screen. A Drawing
  "on 2s" has an Exposure of two frames. See
  [Change an Exposure](05-timeline-and-xsheet.md#timeline-change-exposure).
- **Flip.** To show Drawings one after another by hand, to check the motion
  while you draw. Scrubbing is different: it moves the playhead along the
  Timeline. See [Flip](06-animating.md#animating-flip).
- **Genga palette.** A built-in palette with the four line colours of genga:
  main, shadow, highlight and second shadow. See
  [The Genga palette](08-anime-workflow.md#anime-genga-palette).
- **Inbetween.** A Drawing that has the Mark Inbetween. It fills the motion
  between Keys and Breakdowns. The program makes a tween. You draw an Inbetween.
  See [Create Inbetweens](06-animating.md#animating-inbetweens).
- **Key.** A Drawing that has the Mark Key. It is a main pose of the motion.
  See [Marks](05-timeline-and-xsheet.md#timeline-marks).
- **Layout.** The arrangement of the Panels in the main window. You can switch
  between the Layouts Draw, Animate and Ink, and reset the current Layout. See
  [Layouts](01-getting-started.md#getting-started-layouts).
- **Linked Drawing.** A Drawing that shares its image with other Drawings.
  Painting on one changes all of them. See
  [Linked Drawings](05-timeline-and-xsheet.md#timeline-linked-drawings).
- **Mark.** An optional label on a Drawing that tells its role in the motion:
  Key, Breakdown or Inbetween. A Drawing has at most one Mark. See
  [Marks](05-timeline-and-xsheet.md#timeline-marks).
- **Marker.** A named, coloured point on the Timeline ruler. A Marker is not a
  Mark. See [Markers](05-timeline-and-xsheet.md#timeline-markers).
- **Mirror.** To turn an image into its left-right or top-bottom reverse. Mirror
  View changes only how the canvas is shown. Mirror Selection changes the
  selected pixels or strokes.
- **Palette.** A named list of colour swatches that belongs to a project. See
  [The Genga palette](08-anime-workflow.md#anime-genga-palette).
- **Panel.** A titled region of the main window that holds one group of
  controls, such as the tools, the colours or the Timeline. A Panel is in the
  main window until you drag it out. Then it is floating. See
  [The main window](01-getting-started.md#getting-started-main-window).
- **Panel field.** A text field of a storyboard panel in the Storyboard Panel:
  Scene, Cut, Notes or Audio. See
  [The Storyboard layer and the Storyboard Panel](08-anime-workflow.md#anime-storyboard).
- **Paper template.** The genga sheet: a white sheet with a margin and a Title
  block, around the camera frame. It shows in the View and in Export on Paper.
  See [The Paper template and the Title block](08-anime-workflow.md#anime-paper-template).
- **Rough layer.** A layer for rough drawings. The program shows it tinted and
  faded under the other layers and never exports it. See
  [Rough layers](04-layers.md#layers-rough).
- **Shift and Trace.** A mode in which you can move and rotate the onion-skinned
  Drawings around the current Drawing in the View, to trace an Inbetween. It
  never changes a Drawing. See
  [Shift and Trace](06-animating.md#animating-shift-trace).
- **Storyboard layer.** A layer whose Drawings are storyboard panels. The
  Exposure of a panel is its time, so playback is an animatic. See
  [Storyboard layers](04-layers.md#layers-storyboard).
- **Stroke tool.** A tool that changes the canvas along the path of the pointer:
  Pencil, Pen, Brush, Eraser and Smudge. See
  [The Stroke tools](02-drawing-tools.md#drawing-tools-stroke-tools).
- **Timesheet.** The layout of the X-sheet that Japanese studios use, with fixed
  Key, Dialogue, In-between and Camera columns. See
  [The Timesheet layout](05-timeline-and-xsheet.md#timeline-timesheet) and
  [Timesheet columns](08-anime-workflow.md#anime-timesheet).
- **Title block.** The fields of the Paper template above the camera frame:
  TITLE, ID, NAME, STATE, DATE and NOTES. See
  [The Paper template and the Title block](08-anime-workflow.md#anime-paper-template).
- **Tool column.** The Panel that shows one icon for each tool. Its title is
  "Tools". See [Tool column](01-getting-started.md#getting-started-tool-column).
- **Tool group.** Tools that share one button in the Tool column. Press the
  button for a long time to see the others. See
  [Tool groups](02-drawing-tools.md#drawing-tools-groups).
- **Tool settings.** The Panel that shows the adjustable values of the current
  tool. See [Tool settings](01-getting-started.md#getting-started-tool-settings).
- **View.** The pan, zoom, rotation and mirror through which the canvas shows. A
  change to the View never changes the drawing. See
  [The View tools](02-drawing-tools.md#drawing-tools-view-tools).
- **View tool.** A tool that changes only the View: Hand, Zoom and Rotate View.
  See [The View tools](02-drawing-tools.md#drawing-tools-view-tools).
- **Welcome screen.** The screen that shows at start when no project is given. It
  has the recent projects, the new-project templates and Open. See
  [The Welcome screen](01-getting-started.md#getting-started-welcome-screen).
- **X-sheet.** A Panel that shows the Timeline as a table, with one column for
  each layer and one row for each frame. See
  [The X-sheet](05-timeline-and-xsheet.md#timeline-xsheet).
