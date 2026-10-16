/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef LAYER_MANAGER_H
#define LAYER_MANAGER_H

#include "basemanager.h"
#include "layer.h"

class LayerBitmap;
class LayerVector;
class LayerCamera;
class LayerSound;
struct TweenState;
struct LayerArrangement;
enum class CameraEasingType;


class LayerManager : public BaseManager
{
    Q_OBJECT

public:
    explicit LayerManager(Editor* editor);
    ~LayerManager() override;
    bool init() override;
    Status load(Object*) override;
    Status save(Object*) override;

    // Layer Management
    Layer* currentLayer();
    Layer* currentLayer(int offset);
    Layer* getLayer(int index);
    LayerCamera* getCameraLayerBelow(int layerIndex) const;
    Layer* findLayerByName(QString sName, Layer::LAYER_TYPE type = Layer::UNDEFINED);
    Layer* findLayerById(int layerId) const;
    Layer* getLastCameraLayer();
    int    currentLayerIndex();
    void   setCurrentLayer(int nIndex);
    void   setCurrentLayer(Layer* layer);
    int    count();

    bool canDeleteLayer(int index) const;
    Status deleteLayer(int index);
    /** Removes the layer at index from the object without deleting it. The caller owns the layer. */
    Layer* takeLayer(int index);
    /** Puts a layer from takeLayer() back at index and makes it the current layer. */
    void insertLayer(int index, Layer* layer);
    Status renameLayer(Layer*, const QString& newName);
    void notifyLayerChanged(Layer*);
    /** Sets how the layer composites over the layers below it. One undo step. */
    void setBlendMode(Layer* layer, BlendMode mode);
    /** Sets the tween of the current layer's key at frame as one undo step; scrubs to frame. */
    void setKeyTween(int frame, bool tween, CameraEasingType easing, const TweenState& state);

    /** Makes a new group that holds the layer. Returns its id, or 0 when the layer is already in a group. One undo step. */
    int createLayerGroup(Layer* layer);
    /** Moves the layer from its group, if any, to just above the top member of the group. One undo step. */
    void addLayerToGroup(Layer* layer, int groupId);
    /** Takes the layer out of its group and moves it above the group when it is between two members. One undo step. */
    void removeLayerFromGroup(Layer* layer);
    /** Hides or shows the rows of the group's members in the timeline. View state: not recorded for undo. */
    void setLayerGroupCollapsed(int groupId, bool collapsed);
    /** Shows or hides all members of the group. One undo step. */
    void setLayerGroupVisible(int groupId, bool visible);
    /** Sets the opacity (0 to 1) the group's composite draws with. One undo step. */
    void setLayerGroupOpacity(int groupId, qreal opacity);
    /** Moves the layer at `from` to just above the layer at `to` (below it when `to` is lower). A member of an expanded
     *  group stays in its group; a collapsed group moves as a block; nothing moves into a group or below the bottom
     *  camera layer. One undo step. Returns false when nothing moves. */
    bool moveLayer(int from, int to);
    /** Sets the layer order and groups, keeping each group's collapsed state. For undo. */
    void applyLayerArrangement(const LayerArrangement& arrangement);

    void gotoNextLayer();
    void gotoPreviouslayer();

    /** Returns a new Layer with the given LAYER_TYPE */
    Layer* createLayer(Layer::LAYER_TYPE type, const QString& strLayerName);
    LayerBitmap* createBitmapLayer(const QString& strLayerName);
    LayerVector* createVectorLayer(const QString& strLayerName);
    LayerCamera* createCameraLayer(const QString& strLayerName);
    LayerSound*  createSoundLayer(const QString& strLayerName);
    /** Adds a copy of `from` with its keys and blend mode on top, as the current layer. */
    Layer* duplicateLayer(Layer* from, const QString& name);

    // KeyFrame Management
    int lastFrameAtFrame(int frameIndex);
    int firstKeyFrameIndex();
    int lastKeyFrameIndex();

    int animationLength(bool includeSounds = true);

    /** Layer::setExposureForSelectedFrames on every layer with selected frames, as one undo step. */
    void setExposureForSelectedFrames(int value, bool hold);
    /** Removes each selected bitmap or vector drawing that repeats the drawing before it, as one undo step.
     *  The drawing before it then holds over its frames. */
    void cleanExposureOfSelectedFrames();

    /** Selects the keyframes from frame `fromFrame` to `toFrame` on the layers from `fromLayer` to `toLayer`.
     *  Deselects the frames of every other layer. The bounds can be in any order. */
    void selectFrames(int fromLayer, int toLayer, int fromFrame, int toFrame);
    void deselectAllFrames();
    /** Moves the selected frames of every layer by `offset` as one undo step.
     *  Stops at frame 1. Moves nothing and returns false when a frame is in the way on any layer. */
    bool moveSelectedFrames(int offset);
    /** Adds a blank keyframe after the current frame of the current layer and moves every later keyframe one frame right.
     *  One undo step. Returns false on a hidden layer. */
    bool rippleInsertFrame();
    /** Deletes the current frame of the current layer: removes the keyframe there, if any, and moves every later
     *  keyframe one frame left, so a hold gets one frame shorter. One undo step. Returns false on a hidden layer,
     *  on the only keyframe or after the last keyframe. */
    bool rippleRemoveFrame();

    /** This should be emitted whenever the animation length frames, eg. adding, removing, duplicating */
    void notifyAnimationLengthChanged();

    QString nameSuggestLayer(const QString& name);
    int getLastLayerIndex() { return count() - 1; }

signals:
    void currentLayerWillChange(int index);
    void currentLayerChanged(int index);
    void layerCountChanged(int count);
    void animationLengthChanged(int length);
    void layerDeleted(int index);

private:
    int getIndex(Layer*) const;
    void recordLayerAdd(const Layer* layer);
    bool commitLayerArrangement(const LayerArrangement& after, const QString& description);

    int mLastCameraLayerIdx = 0;
};

#endif
