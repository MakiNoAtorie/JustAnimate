/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef COMMANDCENTER_H
#define COMMANDCENTER_H

#include <QObject>
#include <QSize>
#include "pencilerror.h"
#include "filetype.h"

class Editor;
class QWidget;
class ExportMovieDialog;
class QProgressDialog;


class ActionCommands : public QObject
{
    Q_OBJECT

public:
    explicit ActionCommands(QWidget* parent);
    virtual ~ActionCommands();

    void setCore(Editor* e) { mEditor = e; }

    // The export texts the manual's shots show too.
    static QString overwriteQuestion(const QString& path);
    static void setUpMovieProgress(QProgressDialog& progress);

    // file
    Status importAnimatedImage();
    Status importMovieVideo();
    Status importSound(FileType type);
    Status exportMovie(bool isGif = false);
    Status exportImageSequence();
    Status exportImage();
    Status exportOnPaper();
    Status exportStoryboard();
    Status exportTimesheet();
    Status exportGif();

    // edit
    void flipSelectionX();
    void flipSelectionY();
    void selectAll();
    void deselectAll();

    // view
    void ZoomIn();
    void ZoomOut();
    void rotateClockwise();
    void rotateCounterClockwise();

    // Animation
    void PlayStop();
    void GotoNextFrame();
    void GotoPrevFrame();
    void GotoNextKeyFrame();
    void GotoPrevKeyFrame();
    Status addNewKey();

    void resetAllTools();

    /** Will insert a keyframe at the current position and push connected frames to the right */
    Status insertKeyFrameAtCurrentPosition();
    void removeKey();
    void duplicateLayer();
    void duplicateKey();
    void moveFrameForward();
    void moveFrameBackward();
    void removeSelectedFrames();
    void reverseSelectedFrames();
    void addExposureToSelectedFrames();
    void subtractExposureFromSelectedFrames();
    /** Adds `value` to the hold of each selected drawing, or sets the hold to `value` when `hold` is true.
     *  With no frame selected, acts on the drawing at the current frame. */
    void exposeSelectedFrames(int value, bool hold = false);
    /** Removes selected drawings that repeat the drawing before them. With no frame selected, acts on the current layer. */
    void cleanExposureOfSelectedFrames();

    // Layer
    Status addNewBitmapLayer();
    Status addNewVectorLayer();
    /** Asks for the name and size in Camera Properties, then adds the layer. */
    Status addNewCameraLayer();
    /** Adds a camera layer of this size; one undo step. */
    Status addNewCameraLayer(const QString& name, QSize size);
    Status addNewSoundLayer();
    Status addNewStoryboardLayer();
    Status deleteCurrentLayer();
    void changeKeyframeLineColor();
    void changeallKeyframeLineColor();

    void setLayerVisibilityIndex(int index);

    // Help
    void help();
    void website();
    void reportbug();
    void checkForUpdates();
    void openTemporaryDirectory();
    void about();

private:
    void showSoundClipWarningIfNeeded();

    Status convertSoundToWav(const QString& filePath);

    Editor* mEditor = nullptr;
    QWidget* mParent = nullptr;

    bool mSuppressSoundWarning = false;
};

#endif // COMMANDCENTER_H
