# Saving and safety

This chapter explains how to keep your work safe. It tells you how to save a
project, what the program does when you close a project with unsaved changes,
how undo works, how autosave works, and how to get a project back after a
crash. It also tells you how JustAnimate and Pencil2D open each other's files.

## Save and Save As {#saving-save}

The **File** menu has the commands to save a project.

![The File menu](images/saving-and-safety-file-menu.png)

- **Save** (Ctrl+S) writes the project to its file. A new project has no file.
  For a new project, Save asks for a name and a folder first, in the **Save
  animation** dialog.
- **Save As...** (Ctrl+Shift+S) writes the project to a file that you choose.
  The project then uses the new file. The old file stays as it was.

The Command bar has a **Save** button too. While the program saves, a progress
bar shows. The window title shows the path of the file.

If the program cannot save, it shows an error message and writes the details to
a log file in its data folder. A save writes the new file first and only then
replaces the old file. A failed save does not damage the old file.

If a sound clip of the project has no file on the disk, the program saves the
project without the audio of that clip. It then lists the clips in the message
**Missing Sound Files**.

### The unsaved-changes indicator {#saving-indicator}

The status bar, at the bottom of the main window, shows if the project has
unsaved changes. The indicator is a save icon at the right end, next to the zoom
controls. The icon is dark when there are unsaved changes and faint when there
are none. The tooltip of the indicator says **This file has unsaved
changes** or **This file has no unsaved changes**. The title of the main window
also shows an asterisk (\*) when there are unsaved changes.

![The status bar indicator with unsaved changes (top) and after a save (bottom)](images/saving-and-safety-unsaved-indicator.png)

If you undo back exactly to the state of the last save, the indicator turns
off.

## File formats: .pclx and .pcl {#saving-formats}

JustAnimate saves a project as one **.pclx** file. A .pclx file is an archive
that holds the project data and the images of all Drawings. This is the same
format as Pencil2D uses.

The older Pencil2D format **.pcl** is a project file with a separate data
folder. JustAnimate opens .pcl files, but it does not save them. Save a legacy
project as a .pclx file:

1. Open the .pcl file.
2. Choose **File > Save As...** and type a name that ends in `.pclx`.

If you choose Save for a .pcl project, the program asks for a .pclx name. If
you try to write a path that ends in `.pcl`, the program refuses and shows the
message **Legacy .pcl projects can be opened but not saved. Save the project as
.pclx.**

![The message for a legacy .pcl save](images/saving-and-safety-legacy-pcl.png)

The **Open** dialog shows three filters: **Project formats** (both types),
**JustAnimate / Pencil2D Project** (.pclx) and **Legacy Project** (.pcl).
If you open a file that the program cannot write to, it shows a warning. You
can work on the project and use Save As to put it in a folder that you can write
to.

## The unsaved-changes prompt {#saving-prompt}

When you do something that closes the project that is open, and the project has
unsaved changes, the program asks first. This happens when you choose **File >
New**, **File > Open**, a project in **File > Open Recent**, or **File > Exit**,
and when you close the window.

![The prompt that shows when a project has unsaved changes](images/saving-and-safety-unsaved-prompt.png)

The prompt says **This animation has been modified. Do you want to save your
changes?** Choose one of three buttons:

- **Save** saves the project and then continues. If the project has no file,
  the program asks for a name first. If you close that dialog without a name,
  nothing is saved and the project stays open.
- **Discard** continues and drops the unsaved changes.
- **Cancel** stops the command. The project stays open and nothing changes.

## Undo and redo {#saving-undo}

Choose **Edit > Undo** (Ctrl+Z) to take back the last change. Choose **Edit >
Redo** (Ctrl+Shift+Z) to do it again. The Command bar has both buttons too.
The commands show the name of the step, for example **Undo Draw**. They are
not available when there is nothing to undo or redo.

![The Edit menu after a change](images/saving-and-safety-edit-menu.png)

- One undo takes back one whole action. A stroke, a move of a selection, a
  change of an Exposure or a new layer is each one step.
- After you undo, a new change removes the steps that you could redo.
- A save does not clear the undo list. You can undo changes that you made
  before the save.
- The undo list is not in the file. When you open a project again, you start
  with an empty list.
- The program keeps the last 100 steps. To change the number, choose **Edit >
  Preferences... > General** and set **Maximum Number of Undo/Redo Steps**,
  from 20 to 250. The program asks you to confirm, because a new limit
  resets the undo list.

Other chapters tell which actions are undo steps, for example
[layers](04-layers.md#layers-undo) and [Exposure
changes](05-timeline-and-xsheet.md#timeline-change-exposure).

## Autosave {#saving-autosave}

JustAnimate has two autosave methods. You set both in **Edit > Preferences... >
Files**, in the group **Autosave documents**.

![The Autosave documents group in the Files page of the Preferences](images/saving-and-safety-autosave.png)

**Autosave by number of modifications.** Tick **Enable autosave by number of
modifications** and set **Number of modifications before autosaving**, from 4
to 8192. After that number of changes, the program saves the project to its
file. It saves in the background, so you can keep drawing. This method is off by
default.

- If the project has no file yet, the program cannot save it. It shows the
  notice **Unsaved project: press Ctrl+S**. Clear **Remind me to save an
  untitled project** if you do not want this notice.
- For a legacy .pcl project, this method asks for a .pclx name, as Save does.
- If a save is still in progress when the number is reached, the next count
  saves the changes.

**Autosave by time.** Tick **Enable autosave by time** and set **Autosave
period (minutes)**, from 1 to 300. This method is on by default, with a period
of 5 minutes. It does not change your project file. It writes a complete backup
to a temporary working folder of the program. The program uses this backup only
to
[restore the project after a crash](#saving-recovery).

Autosave by time works for a project that has no file, too. A normal Save
removes the backup that the project does not need anymore.

An autosave is not a replacement for Save. Autosave by number overwrites your
project file, and you cannot ask the program for the older version. Use **Save
As...** to keep versions, for example `scene-01-v2.pclx`.

## Crash recovery {#saving-recovery}

If JustAnimate closes in a wrong way (a crash, a power failure or a task kill),
it keeps the last completed backup of Autosave by time. The next time that you
start the program, it looks for a backup before it opens anything else.

![The Restore project dialog](images/saving-and-safety-recovery.png)

The dialog **Restore Project?** says **JustAnimate didn't close correctly. Would
you like to restore the project?** and shows the name of the project. It has
three buttons:

- **Open** opens the backup as the project that is open.
- **Discard** deletes the backup.
- **Cancel** does nothing. The backup stays, and the dialog shows again at the
  next start.

After a good restore, the message **Recovery Succeeded!** tells you to save your
work at once. Do this: choose **File > Save As...** and give the project a name.
The program also asks you to save the recovered project before it closes. If the
backup is damaged, the message **Recovery Failed.** shows, and your project
file is not changed.

The backup has the state of the last autosave. The work you did after that is
lost. With the default period, you lose at most 5 minutes. Do not turn off
Autosave by time if you want this protection. Your project file from the last
Save is not changed by the restore.

## Open Pencil2D files {#saving-pencil2d}

JustAnimate uses the Pencil2D file format, so the two programs open each
other's files.

- **Open** a Pencil2D .pclx or .pcl file as you open a JustAnimate file. The
  program reads it in full.
- To use only some layers of a Pencil2D project in the project that is open,
  choose **File > Import > Layers from Project file...**. Click **Select
  File**, select one or more layers from the list, and click **Import layers**.
  The layers are added to the project and the command is one undo step.

![The Import Layers dialog](images/saving-and-safety-import-layers.png)

JustAnimate adds some data that Pencil2D does not know. A JustAnimate file
still opens in Pencil2D, but Pencil2D handles the new data in this way:

| JustAnimate data | What Pencil2D does |
|---|---|
| [Layer groups](04-layers.md#layers-groups) | Shows the layers with no groups. If you save the file in Pencil2D, the groups are lost. |
| [Tweens](06-animating.md#animating-tween) | Does not interpolate. If you save the file in Pencil2D, the tweens are lost. |
| [Blend modes](04-layers.md#layers-blend), [Lock Transparency](04-layers.md#layers-lock-transparency) | Does not use them. |
| [Rough layers](04-layers.md#layers-rough) and [Storyboard layers](04-layers.md#layers-storyboard) | Shows them as normal layers. |
| [Lock-all](04-layers.md#layers-lock-all) | Ignores it. |
| [Marks](05-timeline-and-xsheet.md#timeline-marks) and [Markers](05-timeline-and-xsheet.md#timeline-markers) | Ignores them. |
| [Linked Drawings](05-timeline-and-xsheet.md#timeline-linked-drawings) | Opens each copy as an independent Drawing. |
| Timesheet columns, paper template and project length | Ignores them. |

The program saves a Linked Drawing as a full image in each place, so no image
is missing when Pencil2D opens the file. Pencil2D only keeps the data that it
knows when it saves a file. Keep the JustAnimate file as your master, and make a
copy before you open a file in Pencil2D.

## What is different from Pencil2D {#saving-differences}

- **Settings and folders.** JustAnimate has its own settings, temporary
  folders and application identity. It does not read the settings of Pencil2D.
  You can run both programs at the same time.
- **Legacy .pcl.** JustAnimate opens .pcl files but does not write them.
- **Autosave.** Autosave by time is on by default, with a period of 5 minutes.
  Autosave by number writes the archive in the background with no progress
  dialog.
- **Safe saves.** A save replaces the old file only when the new file is
  complete. A failed autosave keeps the previous file.
- **Crash recovery.** The program keeps the last completed autosave in a
  recovery folder, so the restore dialog shows at the next start even when the
  temporary folder was cleaned.
- **Layers, timing and tweens.** The data in the table above is only in
  JustAnimate files.
