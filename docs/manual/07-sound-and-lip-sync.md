# Sound and lip-sync

This chapter shows how to put sound in a project and how to match a mouth to a voice. You import a sound clip, look at its waveform, listen to it while you scrub, and put lip-sync markers on the frames where the mouth changes. Then you play the project and export it with the sound.

## Import sound {#sound-import}

Sound goes on a Sound layer. A Sound layer holds sound clips and has no picture. See [Layer types](04-layers.md#layers-types).

![The File > Import menu with the Sound... and Movie Audio... commands](images/sound-and-lip-sync-import-menu.png)

To import a sound clip:

1. Make a Sound layer the current layer. To add one, choose Layer > New Sound Layer. See [Add a layer](04-layers.md#layers-add).
2. Click the frame where the sound must start. If a clip starts on this frame, JustAnimate uses the first later frame where no clip starts.
3. Choose File > Import > Sound.... A box named **Import sound** opens.
4. Choose a file and click Open. The box accepts WAV, MP3, WMA, OGG, FLAC, Opus, AIFF, AAC and CAF files.

If the current layer is not a Sound layer, JustAnimate asks if it must make one. Click **Create sound layer** to make a new Sound layer and continue. Click **Don't create layer** to cancel the import.

A progress box named **Importing sound...** shows while JustAnimate converts the file. JustAnimate always converts the file to WAV and keeps a copy of it in the project. The original file is not used again, so you can move or delete it.

To use the sound of a video file, choose File > Import > Movie Audio.... The steps are the same, and the clip holds only the sound of the movie.

The clip starts on the frame you chose. It covers as many frames as it lasts at the frame rate of the project. Undo removes the whole import in one step.

You can change the clip after the import:

- To move the clip, drag it in the Timeline.
- To copy the clip, choose Animation > Duplicate drawing. The copy goes after the clip.
- To remove the clip, choose Animation > Remove drawing.

A movie export accepts at most 63 sound clips. See [Sound in playback and export](#sound-playback).

## Waveforms {#sound-waveform}

![A Sound layer in the Timeline with its waveform](images/sound-and-lip-sync-waveform.png)

A clip shows its waveform in the Timeline. The waveform is a picture of the loudness of the sound. Each frame has one thin bar. A tall bar is a loud frame. A flat bar is a quiet frame.

The bars line up with the frames, so you can see which frame has a sound. For example, find the start of a word as the first tall bar after a flat part. Click that frame to go to it.

The waveform appears when JustAnimate finishes the decode of the clip. A long clip can need a short time. The Timeline then redraws by itself.

## Scrubbing with sound {#sound-scrub}

To scrub is to drag the playhead along the frame ruler at the top of the Timeline. Click a frame number to go to that frame.

![The Timeline after a scrub to frame 9](images/sound-and-lip-sync-scrub.png)

With **Sound scrub** on, JustAnimate plays a short piece of the sound each time you go to a new frame. A new frame restarts the piece, so you can find a sound when you drag fast. The Next Frame and Previous Frame commands play the piece too.

To turn Sound scrub on or off, click the **Sound scrub on/off** button in the playback controls of the Timeline. The button has a wave icon.

![The playback controls of the Timeline](images/sound-and-lip-sync-scrub-controls.png)

To set the length of the piece:

1. Choose Edit > Preferences....
2. Click **Timeline**.
3. Use the slider or the box in the **Sound scrub** group. The value is in milliseconds (ms). The smallest value is the length of one frame, or 100 ms if a frame is longer. The largest value is 200 ms. The default is 100 ms.

![The Sound scrub group in the Timeline page of Preferences](images/sound-and-lip-sync-scrub-preference.png)

Sound scrub is off at first. It plays only the clips of Sound layers that are visible.

## Lip-sync markers {#sound-lip-sync}

A lip-sync marker is a flag on one frame of a Sound layer. It has a name and a colour. Use the name for the phoneme (a speech sound) that the mouth makes on that frame. For example, use "A", "E", "O" or "MBP".

Put a marker on each frame where the mouth must change. Then draw the matching mouth Drawing on that frame.

![A Sound layer with the markers A, E, O and MBP over the waveform](images/sound-and-lip-sync-markers.png)

Each marker shows a coloured bar at the left of its cell and its name over the waveform. Each Sound layer has its own markers. In the picture, "Start" and "Hit" are markers of the scene, not lip-sync markers.

To work with the markers of a Sound layer, right-click a cell of the layer. The menu applies to the frame you click.

![The marker menu of a Sound layer](images/sound-and-lip-sync-marker-menu.png)

- **Add Marker...** puts a marker on the frame. A box named **Marker** asks for the **Name**. This command shows only when the frame has no marker.
- **Rename Marker...** changes the name of the marker on the frame.
- **Change Marker Color...** opens a colour box and sets the colour of the marker.
- **Move Marker...** asks for the frame in **To frame:** and moves the marker there. A marker that is already on that frame is replaced.
- **Remove Marker** removes the marker.

Each of these changes is one undo step.

Do not mix the lip-sync markers with the markers of the scene. Right-click the frame ruler to add a marker of the scene. A marker of the scene shows as a coloured bar under the ruler and applies to all layers. A lip-sync marker shows only on its Sound layer.

JustAnimate saves the markers in the project file. When you move a clip, its markers move with it. When you load a project whose marker is on a layer that no longer exists, the marker becomes a marker of the scene.

## Sound in playback and export {#sound-playback}

### Playback {#sound-playback-play}

When you play, each clip starts on the frame where it begins. If you start the playback after the beginning of a clip, the clip starts at the correct place in its sound.

The **Sound on/off** button in the playback controls turns all sound on or off. A sound is off when you click the button and it is no longer pressed. A Sound layer whose eye is closed is silent too.

### Loops and ranges {#sound-playback-loop}

![The playback controls with Loop and a playback range from frame 5 to frame 17](images/sound-and-lip-sync-loop.png)

Use the **Loop** button to repeat the playback. Use the **Range** box, and the start and end boxes next to it, to play only some frames. See [Play-around and the loop range](06-animating.md#animating-range).

- When the playback loops, the sound restarts on the first frame of the loop. It does not continue from the end of the last turn.
- A clip that starts after the first frame of the range starts when the playback reaches it. A clip that is already running at the first frame starts from the correct place in its sound.
- When the playback reaches the last frame of the range, the sound stops.

The preview and the export use the same rules, so the sound you hear is the sound in the movie.

### Export {#sound-playback-export}

To export a movie with sound, choose File > Export > Movie.... JustAnimate mixes the clips of all visible Sound layers into one sound track. A Sound layer that is hidden is not in the movie.

A clip can end after the last Drawing. To include all the sound, tick **To the end of sound clips** in the export box. The end frame then changes to the end of the last clip. If you do not tick the box, the movie stops at the last Drawing.

A movie export accepts at most 63 sound clips. With more clips, JustAnimate shows an error and does not export. Split a long project into shorter projects to stay in the limit. An animated GIF has no sound.

## Missing sound files {#sound-missing}

A sound clip points to a file in the project. If that file is not there, the clip is missing its sound. This can occur when the file is deleted from the data folder of the project.

JustAnimate does not stop when a file is missing. It shows a box named **Missing Sound Files** and goes on without that clip.

![The warning for a sound clip whose file is missing](images/sound-and-lip-sync-missing.png)

- When you save, the box says that the clips were saved without audio. The save does not fail.
- When you export a movie, the box says that the clips were left out of the movie. The export does not fail.

The box names each clip with the name of its layer and its frame, for example "Voice frame 1".

To fix a missing clip, remove the clip and import the sound file again. Do this before you save, because a saved clip has no audio.
