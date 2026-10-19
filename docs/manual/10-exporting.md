# Exporting

This chapter shows how to get your animation out of JustAnimate. You can make a movie, an animated GIF, one picture, a group of pictures, or one sprite sheet. You can also export from the command line, without the window. At the end, the chapter lists what an export leaves out.

The export commands are in the File > Export menu. This chapter covers Movie..., Animated GIF..., Image..., Image Sequence... and On Paper.... The Palette... and Brush Preset... commands save a palette and a brush preset, not pictures.

![The File > Export menu](images/exporting-menu.png)

Each command opens a box with the options. Most boxes have these parts:

- **File.** The name of the output file. Click **Browse...** to choose a folder and a name.
- **Camera.** The Camera layer to export. The picture is what that camera sees. See [The camera layer](06-animating.md#animating-camera).
- **Resolution.** The **Width** and the **Height** in pixels. The default is the size of the camera frame.
- **Range.** The first and last frame. See [Export ranges](#exporting-range).

## Export a movie {#exporting-movie}

To export a movie, choose File > Export > Movie... (Ctrl+M). The box is named **Export Movie**.

![The Export Movie box for an MP4 file](images/exporting-movie-mp4.png)

The extension of the file name sets the type of movie. In the **Browse...** box, choose one of these types:

- **MP4.** The best choice for most uses. It plays almost everywhere.
- **WebM.** A movie for web pages. It can have transparency.
- **AVI.** An older type of movie.
- **APNG.** An animated picture. It can have transparency and it can loop.

The movie uses the frame rate of the project. JustAnimate mixes the sound of the visible Sound layers into the movie. See [Export a movie with sound](07-sound-and-lip-sync.md#sound-playback-export).

Movie export uses the FFmpeg program. If JustAnimate cannot find FFmpeg, a box with the title **Ooops, Something went wrong.** opens and no file is made. Install FFmpeg and try again.

![The box that opens when FFmpeg is missing](images/exporting-ffmpeg-missing.png)

### Presets {#exporting-presets}

The **Quality** group has a list with three presets: **Low**, **Medium** and **High**. The default is Medium. The list is on for MP4 and WebM files only.

- Low makes the smallest file and the lowest picture quality.
- High makes the largest file and the best picture quality.

Use High for a final movie. Use Low for a quick test.

The other parts of the **Quality** group (**Colors** and **Dither**) are for GIF files. See [Animated GIF](#exporting-gif).

### Transparency and loop {#exporting-movie-options}

The **Exporter Settings** group has the **Transparency** box. It is on for WebM and APNG files only. When you tick it, the parts of the picture with no Drawing are clear. For other types, those parts are white.

The **Loop** box in the **Range** group is on for APNG and GIF files only. A file that loops starts again when it ends.

![The Export Movie box for a WebM file, with Transparency ticked](images/exporting-movie-webm.png)

### Even size for MP4 {#exporting-even}

The MP4 type does not accept an odd width or an odd height. If you type an odd number, a mark shows next to the box and the **OK** button turns off. Hold the pointer on the mark to read the reason. Type an even number, or choose a different type of file.

![The Export Movie box with an odd width](images/exporting-movie-odd.png)

## Animated GIF {#exporting-gif}

To export an animated GIF, choose File > Export > Animated GIF... (Ctrl+G). The box is named **Export Animated GIF**.

![The Export Animated GIF box with 64 colors](images/exporting-gif.png)

A GIF file can have at most 256 colors. JustAnimate makes one palette of colors for the whole animation. Two options change how the palette is used:

- **Colors.** The number of colors in the palette, from 3 to 256. The default is 256. A smaller number makes a smaller file, but the colors look flatter.
- **Dither.** On by default. Dither mixes dots of two palette colors to show a color that the palette does not have. It makes smooth colors look better. It also makes the file larger. Clear the box for flat, clean areas, such as a drawing with few colors.

Tick **Loop** to make the GIF start again when it ends. A GIF has no sound and no transparency.

## Export an image {#exporting-image}

To export the picture of the current frame, choose File > Export > Image... (Ctrl+Shift+R). The box is named **Export image**. The box has no range, because it exports one frame only.

![The Export image box](images/exporting-image.png)

Choose a **Format**: PNG, JPG, BMP, TIFF or WEBP.

Tick **Transparency** to leave the parts with no Drawing clear. JPG and BMP files cannot be clear, so the box is off for them and those parts are white. Use PNG when you need transparency.

## Export an image sequence {#exporting-sequence}

To export one picture file for each frame, choose File > Export > Image Sequence... (Ctrl+R). The box is named **Export image sequence**.

![The Export image sequence box](images/exporting-sequence.png)

The file name that you choose is the start of each name. JustAnimate adds the frame number with four digits. For example, the file `ball.png` gives `ball0001.png`, `ball0002.png` and so on. The **Format** and **Transparency** options are the same as for one image.

Tick **Export Drawings only** to export only the frames where the current layer has a Drawing. The frames that only show an earlier Drawing are not exported. See [Drawings and Exposure](05-timeline-and-xsheet.md#timeline-drawings-exposure).

### Sprite sheet {#exporting-sprite-sheet}

A sprite sheet is one picture that holds all the frames in a grid. Game tools use it. Tick **Sprite sheet (one PNG)** in the image sequence box.

![The Export image sequence box with a sprite sheet](images/exporting-sprite-sheet.png)

- The file is always one PNG. The **Format** list and **Export Drawings only** turn off.
- The frames go in the grid from left to right, then from top to bottom. Each cell has the size in **Resolution**.
- Use the **Columns** box to set the number of columns. The default, **Columns: automatic**, makes a grid that is nearly square.
- **Transparency** makes the empty parts of the sheet clear.

## Export ranges {#exporting-range}

The **Range** group sets the frames to export. Movies, GIF files, image sequences and sprite sheets have it.

![The Range group, with a start frame of 5 and an end at the end of the sound](images/exporting-range.png)

- **Start Frame.** The first frame. The default is 1.
- **End Frame.** The last frame. The default is the last frame with a Drawing.
- **To the end of sound clips.** Tick this box to set **End Frame** to the end of the last sound clip. Clear it to go back to the last Drawing.

To export one part of a long animation, type its first and last frame. For example, a start of 5 and an end of 18 gives 14 frames.

## Overwrite and cancel {#exporting-overwrite}

If the output file is already there, JustAnimate asks before it writes. For an image sequence, it asks once, when at least one of the files exists.

![The question when the file exists](images/exporting-overwrite.png)

Click **Yes** to replace the files. Click **No** to cancel the export. No file changes.

A progress box shows while a movie, an image sequence or a sprite sheet is made. Click **Abort** to stop.

![The progress box with the Abort button](images/exporting-progress.png)

JustAnimate writes the new files with temporary names and moves them to their final names only when the export is complete. When you click **Abort**, or when an error stops the export, you get no new files and the old files stay. When a movie is complete, a box asks if you want to open it. For a GIF file, the box asks if you want to open the folder.

If the file of a sound clip is missing, the movie is made without that clip and a box named **Missing Sound Files** lists the clips. See [Missing sound files](07-sound-and-lip-sync.md#sound-missing).

## Export from the command line {#exporting-cli}

You can export without the window. Start JustAnimate with the project and one or more output files. The program exports, prints messages and quits.

```
pencil2d project.pclx -o ball.mp4
pencil2d project.pclx -o ball.png -o ball.gif --start 5 --end 18
```

The extension of each output file sets the type:

- A movie extension (MP4, AVI, WebM, APNG or GIF) makes a movie at the frame rate of the project.
- An image extension (PNG, JPG, TIFF, BMP or WEBP) makes an image sequence, with the numbers in the names as in [Export an image sequence](#exporting-sequence).
- Any other extension gives a warning and the output is PNG.

The options are:

| Option | Meaning |
|---|---|
| `-o <path>`, `--export <path>` | An output file. Use the option more than once for more files. |
| `--camera <name>` | The Camera layer. If the name is not found, JustAnimate uses the first camera and prints a warning. |
| `--width <n>`, `--height <n>` | The size in pixels. The default is the size of the camera frame. |
| `--start <n>` | The first frame. The default is 1. |
| `--end <n>` | The last frame. Use `last` for the last Drawing (default) or `last-sound` for the end of the last sound clip. |
| `--transparency` | Leave clear the parts with no Drawing, for image sequences. |
| `--paper A4` or `--paper B4` | Export on paper. See [What an export leaves out](#exporting-left-out). |

The command line has no options for the movie quality, the GIF colors, the sprite sheet or Loop. It uses the defaults of the boxes: Medium quality, 256 colors and Dither on. It does not ask before it replaces a file.

Transparency does not apply to movies on the command line. If you use `--transparency` with a movie, JustAnimate prints a warning. A value that is not a number gives a warning and JustAnimate uses the default.

### Exit codes {#exporting-exit-codes}

Messages go to the error output. The exit code tells a script what occurred:

| Code | Meaning |
|---|---|
| 0 | All the files are made. |
| 1 | The export failed. |

The export fails, and no more files are made, when:

- no project file is given, or JustAnimate cannot open the project;
- the project has no Camera layer;
- the frame range is empty, for example when `--start` is after the last frame of the animation;
- FFmpeg is not found (the program says so in a message);
- a file cannot be written.

Each error message starts with `Error:`. If you export more than one file, the program stops at the first error.

## What an export leaves out {#exporting-left-out}

Two things that you see on the canvas are not in the exported files:

- **Rough layers.** A Rough layer is never exported. Draw the clean lines on a layer above it. See [Rough layers](04-layers.md#layers-rough).
- **The Paper template.** The Paper template is the genga sheet and title block around the camera frame. It is project data that shows on the canvas. Movies, GIF files, images, image sequences and sprite sheets show only what the camera sees, so they do not have it.

To change the Paper template, choose View > Paper Template....

![The Paper Template box](images/exporting-paper-template.png)

To print the Paper template, choose File > Export > On Paper.... Each frame goes on its own sheet, in a PDF file or in PNG pages. You choose A4 or B4 paper (landscape), the camera and the range of frames. From the command line, use `--paper A4` or `--paper B4` with an output file that ends in `.pdf` or `.png`.

To read about the other parts of the program, go back to the [start page](00-index.md#manual-start).
