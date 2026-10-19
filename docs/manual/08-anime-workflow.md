# Anime workflow

This chapter shows the tools for the way that Japanese animation studios work.
The sections follow the order of a job: start a project from a template, draw on
a genga sheet with the genga colours, plan the timing on a Timesheet, plan the
story on a storyboard, and print the sheets on paper.

You can use each tool alone. For the basics of Drawings and Exposure, read
[Drawings and Exposure](05-timeline-and-xsheet.md#timeline-drawings-exposure).

## Anime templates {#anime-templates}

A template sets the size of the picture and the frames per second (fps) of a new
project. Two templates are for anime:

- **Anime 1080p** is 1920 x 1080 pixels at 24 fps.
- **Anime 720p** is 1280 x 720 pixels at 24 fps.

To use a template:

1. Start JustAnimate, or choose File > New. The Welcome screen opens.
2. In the list of templates, choose **Anime 1080p** or **Anime 720p**.
3. Click **New**.

![The Welcome screen with the Anime 1080p template chosen](images/anime-workflow-templates.png)

The new project has a Camera layer and a Bitmap layer. The camera frame has the
size of the template. For more about the Welcome screen, read
[The Welcome screen](01-getting-started.md#getting-started-welcome-screen).

To change the fps later, use the fps box in the Timeline.

## The Paper template and the Title block {#anime-paper-template}

The Paper template is the genga sheet. It is a white sheet that JustAnimate
draws around the camera frame. The sheet has a margin around the frame and a
**Title block** above the frame. The Paper template is a part of the project.
JustAnimate saves it with the project file.

The Title block has these fields:

| Field | Use |
|---|---|
| TITLE | The name of the production. |
| ID | The number of the cut. |
| NAME | The name of the animator. |
| STATE | The state of the work, for example "Genga". |
| DATE | The date of the sheet. |
| NOTES | Notes for the next person. NOTES holds three lines. |

To set the Paper template:

1. Choose View > Paper Template....
2. Select **Show the genga sheet around the camera frame**.
3. Enter the text of the fields in **Title**, **ID**, **Name**, **State**,
   **Date** and **Notes**.
4. Click **OK**.

![The Paper Template dialog with a Title block](images/anime-workflow-paper-dialog.png)

The sheet shows in the View, around the camera frame. It follows the camera when
the camera moves. Each field you change is one step in the undo list.

![The View with the Paper template around the camera frame](images/anime-workflow-paper-template.png)

The sheet is a guide for you while you draw. It does not show in the image and
movie exports. Only [Export on Paper](#anime-export-paper) prints it. To hide the
sheet, clear the box **Show the genga sheet around the camera frame**. The
Title block stays in the project.

## The Genga palette {#anime-genga-palette}

The Genga palette is a built-in palette with the four line colours that studios
use for genga:

| Colour name | Colour | Use |
|---|---|---|
| Main line | Black | The main lines of the drawing. |
| Shadow line | Red | The border of a shadow. |
| Highlight line | Blue | The border of a highlight. |
| Second shadow line | Green | The border of a second, darker shadow. |

To load the palette:

1. Right-click a swatch in the **Palette** Panel.
2. Choose **Built-in Palettes**, then **Genga**.
3. If strokes use the old palette, JustAnimate shows a message. The strokes
   change to the new colours at the same palette numbers. Click **Load Palette**
   to continue, or **Cancel** to keep the old palette.

![The Palette Panel after you load the Genga palette](images/anime-workflow-genga-palette.png)

The Genga palette replaces the whole palette of the project. Choose **Default**
in the same submenu to go back to the default colours.

## Timesheet columns {#anime-timesheet}

The Timesheet is the layout of the X-sheet that Japanese studios use. It has
fixed columns: **Key A** to **Key F**, **Dialogue**, **In-between A** to
**In-between F** and **Camera**. One sheet holds 144 frames.

To show the Timesheet, click **Timesheet** in the toolbar of the **X-sheet**
Panel. Click it again to go back to one column for each layer.

JustAnimate puts each layer in a column by itself:

- A sound layer goes to **Dialogue**.
- A Camera layer, or any layer that holds no bitmap or vector Drawings, goes to
  **Camera**.
- The bitmap and vector layers go to **Key A**, **Key B** and so on, from the
  bottom layer up. After **Key F**, they go to **In-between A** and so on.

To choose the column yourself:

1. Right-click the layer name in the Timeline or the X-sheet.
2. Choose **Timesheet Column**.
3. Choose a column. A sound layer always stays in **Dialogue**.

![The Timesheet Column submenu of a layer](images/anime-workflow-timesheet-menu.png)

Choose **From layer order** to give the choice back to JustAnimate. If more than
one layer is in a column, the X-sheet shows all of them side by side, from the
bottom layer up. In the picture below, the layer Line is in **In-between A**.

![The X-sheet Panel in the Timesheet layout](images/anime-workflow-timesheet.png)

For more about the X-sheet, read
[The Timesheet layout](05-timeline-and-xsheet.md#timeline-timesheet).

## The Storyboard layer and the Storyboard Panel {#anime-storyboard}

A **Storyboard layer** is a layer whose Drawings are the panels of a
storyboard. The Exposure of a panel is its time. When you play the project, you
see the storyboard as a timed film.

To make a Storyboard layer, do one of these:

- Choose Layer > New Storyboard Layer.
- Right-click a layer name and choose **Storyboard Layer**.
- Open the **Storyboard** Panel and click **Make current layer the Storyboard
  layer**.

Draw one Drawing for each panel. Give each Drawing the Exposure that the panel
needs.

The **Storyboard** Panel (Window > Storyboard, or Ctrl+0) shows one row for each
panel. These are the columns:

| Column | Use |
|---|---|
| Scene | The scene number. |
| Cut | The cut number. |
| Notes | The action and the notes for the panel. |
| Audio | The dialogue and sound for the panel. |
| Time | The Exposure of the panel, in seconds and frames. |

![The Storyboard Panel with four panels](images/anime-workflow-storyboard.png)

To edit a field, double-click it and enter the text. To change the time of a
panel, edit **Time**. The next panels move with it. The last panel holds to the
end of the project, so its time sets the length of the project. Click a row to
move the playhead to that panel. Each edit is one step in the undo list.

If a project has more than one Storyboard layer, the Panel shows the current
layer when it is a Storyboard layer. If not, it shows the first one.

## Export Timesheet {#anime-export-timesheet}

Choose File > Export > Timesheet... to print the Timesheet on paper.

1. Choose the paper in **Paper**: **A4 portrait** or **B4 portrait**. B4 is the
   JIS B4 size (257 x 364 mm).
2. Check **Scene**, **Shot** and **Animator**. JustAnimate fills them from the
   Title block: Animator comes from NAME. Scene and Shot come from ID when ID
   has the form "scene-shot", for example "12-034". Enter other text if you
   need it. JustAnimate remembers the last text that you entered.
3. Click **OK**.
4. Enter a file name. Choose **PDF** for one file with all sheets, or **PNG
   pages** for one image file for each sheet.

![A page of the exported Timesheet](images/anime-workflow-export-timesheet.png)

Each sheet holds 144 frames, in two halves of 72 frames. A project of more than
144 frames makes more sheets. The header of a sheet shows TITLE from the Title
block, SCENE, SHOT, ANIMATOR, the TIME in seconds and frames, and SHEET NO. n / N.
Each column shows the layer that is highest in the column. Read
[Timesheet columns](#anime-timesheet) to put the layers in the right columns.

## Export Storyboard {#anime-export-storyboard}

Choose File > Export > Storyboard... to print the Storyboard layer.

1. Choose the file name and the type: **PDF** or **PNG pages**.
2. Wait for the pages. A progress box shows the page number. Click **Abort** to
   stop. An aborted export writes no files.

The export needs a Storyboard layer and a Camera layer. If one is missing,
JustAnimate shows a message that says which one.

![A page of the exported storyboard](images/anime-workflow-export-storyboard.png)

The pages are A4 portrait. Each page holds five panels. A row has these cells: S
(Scene), C (Cut), PICTURE, NOTES, AUDIO and TIME. The PICTURE cell shows the
first frame of the panel, as the camera sees it. TIME is the time of the panel
in seconds and frames. The page header shows PRODUCTION, from the TITLE of the
Title block, and PAGE n / N. The export uses the first Storyboard layer of the
project.

## Export on Paper {#anime-export-paper}

Choose File > Export > On Paper... to print frames on the Paper template. Use it
for the genga sheets that you give to the next person.

1. Choose the paper in **Paper**: **A4 landscape** or **B4 landscape**.
2. Choose the camera in **Camera**.
3. Enter the first and the last frame in **From frame** and **To frame**.
4. Click **OK**.
5. Enter a file name and choose **PDF** or **PNG pages**.

![A page of Export on Paper](images/anime-workflow-export-paper.png)

Each page is one frame. It shows the Drawings of that frame in the sheet, with
the Title block. The export prints the Paper template even when the sheet is
hidden in the View. If the Title block is empty, the fields on the page are
empty.

For PNG pages, the file names get a number: `name_0001.png`, `name_0002.png`
and so on. For PDF, all pages are in one file. If a write fails, the old files
stay as they were.

## Common problems {#anime-problems}

- **The sheet does not show in the View.** Choose View > Paper Template... and
  select **Show the genga sheet around the camera frame**.
- **A column of the Timesheet is empty.** No layer is in that column. Right-click
  a layer and choose the column in **Timesheet Column**.
- **The Storyboard Panel says "No Storyboard layer yet".** Choose
  Layer > New Storyboard Layer, or click **Make current layer the Storyboard
  layer**.
- **Export on Paper says to add a camera layer.** Choose Layer > New Camera Layer.
  The pages show the frame of the camera.
