# Getting started

This chapter shows how to install JustAnimate and make a short animation, from the Welcome screen to an exported movie.

## Install JustAnimate {#getting-started-install}

Get the package for your system from the JustAnimate project page at <https://github.com/makinoatorie/justanimate>.

- **Windows.** Run the installer (`.exe`) and follow the steps. You can also unpack the `.zip` file into a folder and run `justanimate.exe` from it. The `.zip` file needs no install.
- **macOS.** Open the `.dmg` disk image. Drag JustAnimate into the Applications folder, then start it from there.
- **Linux.** Download the `.AppImage` file. Make it executable (`chmod +x`), then run it. The AppImage needs no install.

JustAnimate opens the project files (`.pclx` and `.pcl`) of Pencil2D. Each package contains this manual. To open it, choose Help > User Manual.

## The Welcome screen {#getting-started-welcome-screen}

When you start JustAnimate without a project, the Welcome screen opens.

![The Welcome screen](images/getting-started-welcome-screen.png)

The Welcome screen has these parts:

- **Recent projects.** Click a project to open it. A project that was moved or deleted shows "(missing)" and you cannot open it. When you have no recent projects, this list is not shown.
- **New project.** Choose a template or a preset in the list, then click **New**. A template sets the size of the picture and the frames per second (fps), for example "Anime 720p (1280 x 720, 24 fps)". The templates are Anime 1080p, Anime 720p, HD 1080p, HD 720p, 4K UHD, Vertical 1080 x 1920, Square 1080 and GIF 480 x 480. Below the templates are Default and the presets that you saved. JustAnimate remembers the last template that you chose.
- **Open...** Choose a project file in a folder.
- **Cancel.** Close the Welcome screen.
- **Don't show this screen again.** Tick this box if you do not want the Welcome screen at the start. The next time, JustAnimate opens the last project. If there is none, it makes a new project from the default preset. You can change this in Preferences.

A new project from a template has two layers: a Camera layer and a Bitmap layer. The Bitmap layer is ready for drawing.

To start another project when the program is open, choose File > New (Ctrl+N). If the Welcome screen is turned off, File > New makes a new project from the default preset.

## The main window {#getting-started-main-window}

![The main window with a project open](images/getting-started-main-window.png)

The main window holds the menu bar, the Command bar, the canvas and several Panels. A Panel is a titled part of the window with one group of controls. The **Layout** is the arrangement of the Panels.

The menu bar has these menus: File, Edit, View, Animation, Tools, Layer, Window and Help.

### Command bar {#getting-started-command-bar}

![The Command bar](images/getting-started-command-bar.png)

The Command bar is the row of buttons under the menu bar. From left to right it has buttons for:

- the file commands New, Open and Save
- Undo and Redo
- Cut, Copy, Paste and Clear Drawing
- Zoom In, Zoom Out and Reset
- Mirror View Horizontally and Mirror View Vertically
- Show Invisible Lines and Show Outlines Only
- the overlays: grid, center, thirds, golden ratio, safe areas and perspective lines

At the right end is the **Search commands** box. See [Command search](#getting-started-command-search).

Hold the pointer over a button to read its name and its shortcut.

### Tool column {#getting-started-tool-column}

![The Tool column](images/getting-started-tool-column.png)

The Tool column is the Panel with one icon for each tool. Its title is "Tools". Click an icon to select that tool. The selected tool is filled with the Accent colour. Some icons hold a group of tools. Press and hold the icon, or right-click it, to show the other tools of the group.

In the default Layout, the Tool column is a tab beside the Brush Presets Panel at the top left. Click the **Tools** tab to show it.

You can also select a tool with the Tools menu or with its shortcut. For example, N selects the Pencil, E selects the Eraser, B selects the Brush and H selects the Hand.

### Tool settings {#getting-started-tool-settings}

![The Tool settings Panel](images/getting-started-tool-settings.png)

The Tool settings Panel shows the values of the current tool. The size (Width) and the Opacity are at the top. The other values depend on the tool. When you select a different tool, the Panel shows the values of that tool. In the default Layout, Tool settings is a column at the left. You can also dock it at the top as a one-row strip.

### Panels on the right {#getting-started-right-panels}

The default Layout has the **Color Box** and the **Palette** at the right. Tabs under the Palette show the **Color Inspector** and the **Storyboard** Panels. Click a swatch in the Palette to select that colour.

### Timeline and canvas {#getting-started-timeline-canvas}

![The Timeline Panel](images/getting-started-timeline.png)

The **Timeline** Panel is at the bottom of the window. It has these parts:

- **Layers.** The names of the layers, one row for each layer. The buttons above the names add, delete and duplicate a layer.
- **Drawings.** The three buttons (New drawing, Remove drawing and Duplicate drawing) change the Drawings of the current layer.
- **Frames.** Each Drawing is shown in the row of its layer, from its first frame to the frame before the next Drawing. The number ruler is above the rows. The blue bar on the ruler is the playhead. It shows the current frame.
- **Playback controls.** The buttons for Play, Loop and the frame steps, the fps box and the playback speed.

Two more tabs are under the Timeline: **X-sheet** and **Onion skin**. They are Panels that share the space of the Timeline.

The canvas is the large area in the middle. In the bottom-left corner of the canvas is the **canvas HUD**. It shows the size of the current tool as a small circle. At the right, it shows the zoom of the View in percent and the rotation of the View in degrees. For example, "100% 0°" means that the View is at normal size and is not rotated. The zoom box and the slider at the bottom right of the window also show and change the zoom.

## Layouts {#getting-started-layouts}

A Layout is a named arrangement of the Panels. JustAnimate has three Layouts: **Draw**, **Animate** and **Ink**. Animate is the Layout that a new installation shows.

To change the Layout, choose Window > Layout and then the name of a Layout.

![The Draw Layout](images/getting-started-layout-draw.png)

- **Draw** hides the Timeline, the X-sheet and the Onion skin Panel. The canvas is larger. Use it to draw one Drawing at a time.

![The Ink Layout](images/getting-started-layout-ink.png)

- **Ink** hides the same Panels and also the Color Inspector and the Palette. Use it when you do not need to change colours.
- **Animate** shows all Panels, with the Timeline. Use it when you work with many frames.

Each Layout remembers the changes that you make to its Panels. You can drag a Panel by its title to a new place, make it float, or close it.

- **Reset layout.** Choose Window > Layout > Reset layout to put the Panels of the current Layout back in their first places.
- **Lock Panels.** Choose Window > Lock Panels to stop the Panels from moving, floating and closing. This prevents accidental moves. Choose the item again to unlock the Panels.

## Command search {#getting-started-command-search}

Command search finds a menu command by its name and runs it. You do not need to know where the command is in the menus.

1. Press Ctrl+Shift+P, choose Edit > Command search..., or click the **Search commands** box in the Command bar.
2. Type a part of the name, for example `export`.
3. Use the Up and Down arrow keys to choose a command, then press Enter. You can also click it.

![Command search with the text "export"](images/getting-started-command-search.png)

Each result shows the menu path of the command and its shortcut. Use the shortcut to run the command faster the next time.

## Theme and Accent colour {#getting-started-theme-accent}

JustAnimate has a light theme and a dark theme. The **Accent colour** marks the selected tool, selections, the slider fill, the playhead and the keyboard focus.

1. Choose Edit > Preferences... (Ctrl+K).
2. Open the **General** page.
3. In the first list that says "Theme", choose System, Light or Dark. System uses the theme of your operating system.
4. In the list that says "Accent", choose Blue, Teal, Green, Amber, Orange, Red, Pink or Violet.

![The Theme and Accent lists in Preferences](images/getting-started-preferences-appearance.png)

The change is applied at once. The next picture shows the Dark theme with the Teal Accent colour.

![The main window with the Dark theme and the Teal Accent colour](images/getting-started-theme-dark.png)

## Draw the first Drawing {#getting-started-first-drawing}

In this part you make a short animation of a ball that moves across the screen. It uses three Drawings.

1. Start JustAnimate. On the Welcome screen, choose **Anime 720p** and click **New**.
2. Click the **Tools** tab, then click the Pencil in the Tool column. You can also press N.
3. In Tool settings, keep the Width at a value that you can see well, for example 4.
4. Make sure that the playhead is on frame 1. Draw a circle in the upper left of the canvas. Hold the pointer button down while you draw, then let it go.

![The first Drawing, a circle, on frame 1](images/getting-started-first-drawing.png)

The Timeline now shows the first Drawing of the Bitmap layer on frame 1. If you make a mistake, choose Edit > Undo (Ctrl+Z) and draw again.

## Add more Drawings {#getting-started-more-drawings}

Each new Drawing is a new image on the same layer. A Drawing stays on the screen until the next Drawing starts. A Drawing that stays for two frames is "on 2s". Its Exposure is two frames.

1. Move the playhead to frame 3. Click frame 3 on the number ruler, or press the full stop (.) two times.
2. Click **New drawing** in the Timeline, or press F7. A blank Drawing is made on frame 3.
3. Draw the circle again, lower and to the right of the first circle.
4. Move the playhead to frame 5. Add a new Drawing and draw the circle again, lower and to the right of the second one.

![Three Drawings on frames 1, 3 and 5](images/getting-started-more-drawings.png)

To see the earlier Drawing while you draw, press O. This turns the onion skin on or off. The onion skin shows the neighbour Drawings in a faint tint, so that you can put the next circle in the right place. To copy a Drawing instead, use **Duplicate drawing** (F6).

## Play the animation {#getting-started-play}

1. Set the fps box in the Timeline to the speed that you want. The Anime 720p template uses 24 fps.
2. Click **Play** in the Timeline, or press Ctrl+Return. The playhead moves across the frames.
3. Click **Play** again to stop.

Click the **Loop** button before you play to repeat the animation without a stop. The **Ping-pong** box plays the animation forward and then backward.

To check the motion by hand, move the playhead one frame at a time with the full stop (.) and the comma (,).

## Save the project {#getting-started-save}

Choose File > Save (Ctrl+S). The first time, JustAnimate asks for a name and a folder. It saves the project as a `.pclx` file. Use File > Save As... (Ctrl+Shift+S) to save a copy with a different name.

A saved project shows in the **Recent projects** list of the Welcome screen and in File > Open Recent.

## Export a movie {#getting-started-export-movie}

1. Choose File > Export > Movie... (Ctrl+M).
2. In the **File** box, type a file name, or click **Browse...** to choose a folder and a name. The file types are MP4, AVI, WebM and APNG. This example uses `ball.mp4`.
3. Check the options in the dialog:
    - **Camera.** The Camera layer to export.
    - **Resolution.** The Width and the Height of the movie, in pixels.
    - **Range.** The **Start Frame** and the **End Frame**. For this example, use 1 and 5.
    - **Quality.** Low, Medium or High.
4. Click **OK**. A progress box shows while the movie is made. When it is finished, JustAnimate asks "Finished. Open movie now?".

![The Export Movie dialog](images/getting-started-export-movie.png)

Movie export uses the FFmpeg program. If the export stops because FFmpeg is missing, install FFmpeg and try again.

To get one picture file for each frame instead of a movie, choose File > Export > Image Sequence... (Ctrl+R).

To read about the other parts of the program, go back to the [start page](00-index.md#manual-start).
