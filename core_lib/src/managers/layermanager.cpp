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

#include "layermanager.h"

#include "object.h"
#include "editor.h"

#include "layersound.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "layercamera.h"
#include "undoredomanager.h"
#include "bitmapimage.h"
#include "vectorimage.h"
#include "scribblearea.h"

#include <algorithm>

#include <QXmlStreamWriter>

LayerManager::LayerManager(Editor* editor) : BaseManager(editor, __FUNCTION__)
{
}

LayerManager::~LayerManager()
{
}

bool LayerManager::init()
{
    return true;
}

Status LayerManager::load(Object*)
{
    mLastCameraLayerIdx = 0;
    // Do not emit layerCountChanged here because the editor has not updated to this object yet
    // Leave that to the caller of this function
    return Status::OK;
}

Status LayerManager::save(Object* o)
{
    o->data()->setCurrentLayer(editor()->currentLayerIndex());
    return Status::OK;
}

LayerCamera* LayerManager::getCameraLayerBelow(int layerIndex) const
{
    return static_cast<LayerCamera*>(object()->getLayerBelow(layerIndex, Layer::CAMERA));
}

Layer* LayerManager::getLastCameraLayer()
{
    Layer* layer = object()->getLayer(mLastCameraLayerIdx);
    if (layer && layer->type() == Layer::CAMERA)
    {
        return layer;
    }

    // it's not a camera layer
    std::vector<LayerCamera*> camLayers = object()->getLayersByType<LayerCamera>();
    if (camLayers.size() > 0)
    {
        return camLayers[0];
    }
    return nullptr;
}

Layer* LayerManager::currentLayer()
{
    Layer* layer = currentLayer(0);
    Q_ASSERT(layer != nullptr);
    return layer;
}

Layer* LayerManager::currentLayer(int incr)
{
    Q_ASSERT(object() != nullptr);
    return object()->getLayer(editor()->currentLayerIndex() + incr);
}

Layer* LayerManager::getLayer(int index)
{
    Q_ASSERT(object() != nullptr);
    return object()->getLayer(index);
}

Layer* LayerManager::findLayerByName(QString sName, Layer::LAYER_TYPE type)
{
    return object()->findLayerByName(sName, type);
}

Layer* LayerManager::findLayerById(int layerId) const
{
    return object()->findLayerById(layerId);
}

int LayerManager::currentLayerIndex()
{
    return editor()->currentLayerIndex();
}

void LayerManager::setCurrentLayer(int layerIndex)
{
    Q_ASSERT(layerIndex >= 0);
    Q_ASSERT(layerIndex < object()->getLayerCount());

    // A box selection can span layers: deselect them all.
    if (currentLayer() != object()->getLayer(layerIndex)) {
        deselectAllFrames();
    }

    emit currentLayerWillChange(layerIndex);

    // Do not check if layer index has changed
    // because the current layer may have changed either way
    editor()->setCurrentLayerIndex(layerIndex);
    emit currentLayerChanged(layerIndex);

    if (object()->getLayer(layerIndex)->type() == Layer::CAMERA)
    {
        mLastCameraLayerIdx = layerIndex;
    }
}

void LayerManager::setCurrentLayer(Layer* layer)
{
    setCurrentLayer(getIndex(layer));
}

void LayerManager::gotoNextLayer()
{
    if (editor()->currentLayerIndex() < object()->getLayerCount() - 1)
    {
        deselectAllFrames();
        editor()->setCurrentLayerIndex(editor()->currentLayerIndex() + 1);
        emit currentLayerChanged(editor()->currentLayerIndex());
    }
}

void LayerManager::gotoPreviouslayer()
{
    if (editor()->currentLayerIndex() > 0)
    {
        deselectAllFrames();
        editor()->setCurrentLayerIndex(editor()->currentLayerIndex() - 1);
        emit currentLayerChanged(editor()->currentLayerIndex());
    }
}

QString LayerManager::nameSuggestLayer(const QString& name)
{
    // if no layers: return name
    if (count() == 0)
    {
        return name;
    }
    QVector<QString> sLayers;
    // fill Vector with layer names
    for (int i = 0; i < count(); i++)
    {
        sLayers.append(getLayer(i)->name());
    }
    // if name is not in list, return name
    if (!sLayers.contains(name))
    {
        return name;
    }
    int newIndex = 2;
    QString newName = name;
    do {
        newName = QStringLiteral("%1 %2")
            .arg(name).arg(QString::number(newIndex++));
    } while (sLayers.contains(newName));
    return newName;
}

Layer* LayerManager::createLayer(Layer::LAYER_TYPE type, const QString& strLayerName)
{
    Layer* layer = nullptr;
    switch (type) {
    case Layer::BITMAP:
        layer = object()->addNewBitmapLayer();
        break;
    case Layer::VECTOR:
        layer = object()->addNewVectorLayer();
        break;
    case Layer::SOUND:
        layer = object()->addNewSoundLayer();
        break;
    case Layer::CAMERA:
        layer = object()->addNewCameraLayer();
        break;
    default:
        Q_ASSERT(true);
        return nullptr;
    }

    layer->setName(strLayerName);
    emit layerCountChanged(count());
    setCurrentLayer(getLastLayerIndex());
    recordLayerAdd(layer);

    return layer;
}

LayerBitmap* LayerManager::createBitmapLayer(const QString& strLayerName)
{
    return static_cast<LayerBitmap*>(createLayer(Layer::BITMAP, strLayerName));
}

LayerVector* LayerManager::createVectorLayer(const QString& strLayerName)
{
    return static_cast<LayerVector*>(createLayer(Layer::VECTOR, strLayerName));
}

LayerCamera* LayerManager::createCameraLayer(const QString& strLayerName)
{
    return static_cast<LayerCamera*>(createLayer(Layer::CAMERA, strLayerName));
}

LayerSound* LayerManager::createSoundLayer(const QString& strLayerName)
{
    return static_cast<LayerSound*>(createLayer(Layer::SOUND, strLayerName));
}

int LayerManager::lastFrameAtFrame(int frameIndex)
{
    Object* o = object();
    for (int i = frameIndex; i >= 0; i -= 1)
    {
        for (int layerIndex = 0; layerIndex < o->getLayerCount(); ++layerIndex)
        {
            auto pLayer = o->getLayer(layerIndex);
            if (pLayer->keyExists(i))
            {
                return i;
            }
        }
    }
    return -1;
}

int LayerManager::firstKeyFrameIndex()
{
    int minPosition = INT_MAX;

    Object* o = object();
    for (int i = 0; i < o->getLayerCount(); ++i)
    {
        Layer* pLayer = o->getLayer(i);

        int position = pLayer->firstKeyFramePosition();
        if (position < minPosition)
        {
            minPosition = position;
        }
    }
    return minPosition;
}

int LayerManager::lastKeyFrameIndex()
{
    int maxPosition = 0;

    for (int i = 0; i < object()->getLayerCount(); ++i)
    {
        Layer* pLayer = object()->getLayer(i);

        int position = pLayer->getMaxKeyFramePosition();
        if (position > maxPosition)
        {
            maxPosition = position;
        }
    }
    return maxPosition;
}

int LayerManager::count()
{
    return object()->getLayerCount();
}

bool LayerManager::canDeleteLayer(int index) const
{
    return object()->canDeleteLayer(index);
}

Status LayerManager::deleteLayer(int index)
{
    Layer* layer = object()->getLayer(index);
    if (layer->type() == Layer::CAMERA)
    {
        std::vector<LayerCamera*> camLayers = object()->getLayersByType<LayerCamera>();
        if (camLayers.size() == 1)
            return Status::ERROR_NEED_AT_LEAST_ONE_CAMERA_LAYER;
    }
    Q_ASSERT(object()->getLayerCount() >= 2);

    layer = takeLayer(index);
    // An editor that is not initialised has no undo manager
    if (editor()->undoRedo())
    {
        editor()->undoRedo()->recordLayerRemove(layer, index, tr("Delete Layer"));
    }
    else
    {
        delete layer;
    }

    return Status::OK;
}

void LayerManager::recordLayerAdd(const Layer* layer)
{
    if (editor()->undoRedo())
    {
        editor()->undoRedo()->recordLayerAdd(layer->id(), tr("Add Layer"));
    }
}

Layer* LayerManager::takeLayer(int index)
{
    Layer* layer = object()->getLayer(index);
    const int current = currentLayerIndex();

    // Resolve a pending selection move while the layer it belongs to is still current
    if (index == current)
    {
        emit currentLayerWillChange(index);
    }

    // current layer is the last layer && we are deleting it
    if (index == object()->getLayerCount() - 1 &&
        index == current)
    {
        setCurrentLayer(current - 1);
    }
    object()->takeLayer(layer->id());
    if (index < current)
    {
        // The current layer moved down one index. It is still the same layer.
        editor()->setCurrentLayerIndex(current - 1);
        emit currentLayerChanged(current - 1);
    }
    else
    {
        // current layer has changed, so trigger updates
        setCurrentLayer(currentLayerIndex());
    }

    emit layerDeleted(index);
    emit layerCountChanged(count());

    return layer;
}

void LayerManager::insertLayer(int index, Layer* layer)
{
    object()->insertLayer(index, layer);
    if (index <= currentLayerIndex())
    {
        // The current layer moved up one index.
        editor()->setCurrentLayerIndex(currentLayerIndex() + 1);
    }

    emit layerCountChanged(count());
    setCurrentLayer(layer);
}

Status LayerManager::renameLayer(Layer* layer, const QString& newName)
{
    if (newName.isEmpty()) return Status::FAIL;

    layer->setName(newName);
    editor()->object()->modification();
    emit currentLayerChanged(getIndex(layer));
    return Status::OK;
}

void LayerManager::notifyLayerChanged(Layer* layer)
{
    emit currentLayerChanged(getIndex(layer));
}

void LayerManager::setBlendMode(Layer* layer, BlendMode mode)
{
    if (layer == nullptr || layer->blendMode() == mode) { return; }
    const BlendMode oldMode = layer->blendMode();
    layer->setBlendMode(mode);
    editor()->undoRedo()->recordLayerBlendMode(layer->id(), oldMode, mode, tr("Blend Mode"));
    notifyLayerChanged(layer);
}

// Moves layers [lo, hi] so the first lands at index t of the list without them.
static void moveLayerBlock(LayerArrangement& a, int lo, int hi, int t)
{
    for (QList<int>* list : { &a.layerIds, &a.groupIds })
    {
        const QList<int> block = list->mid(lo, hi - lo + 1);
        list->erase(list->begin() + lo, list->begin() + hi + 1);
        for (int i = 0; i < block.size(); ++i) { list->insert(t + i, block[i]); }
    }
}

bool LayerManager::commitLayerArrangement(const LayerArrangement& after, const QString& description)
{
    const LayerArrangement before = object()->layerArrangement();
    if (after == before) { return false; }
    applyLayerArrangement(after);
    editor()->undoRedo()->recordLayerArrangement(before, description);
    return true;
}

void LayerManager::applyLayerArrangement(const LayerArrangement& arrangement)
{
    LayerArrangement a = arrangement;
    for (LayerGroup& group : a.groups)
    {
        if (const LayerGroup* now = object()->findLayerGroup(group.id)) { group.collapsed = now->collapsed; }
    }
    Layer* current = currentLayer();
    object()->setLayerArrangement(a);
    int index = getIndex(current);
    if (object()->isLayerFolded(index)) { index = object()->layerGroupRange(current->groupId()).second; }
    setCurrentLayer(index);
    emit editor()->updateTimeLine();
    editor()->getScribbleArea()->onLayerChanged();
}

int LayerManager::createLayerGroup(Layer* layer)
{
    const int index = getIndex(layer);
    if (index < 0 || layer->groupId() != 0) { return 0; }
    LayerArrangement a = object()->layerArrangement();
    const int id = object()->getUniqueLayerGroupID();
    a.groups.append(LayerGroup{ id, tr("Group %1").arg(id) });
    a.groupIds[index] = id;
    commitLayerArrangement(a, tr("New Layer Group"));
    return id;
}

void LayerManager::addLayerToGroup(Layer* layer, int groupId)
{
    if (getIndex(layer) < 0 || layer->groupId() == groupId || object()->layerGroupRange(groupId).first < 0) { return; }
    if (layer->type() == Layer::CAMERA && getIndex(layer) == 0) { return; } // the bottom camera layer does not move

    UndoMacro macro(editor()->undoRedo(), tr("Add Layer to Group"));
    if (layer->groupId() != 0) { removeLayerFromGroup(layer); }
    const int from = getIndex(layer);
    const int top = object()->layerGroupRange(groupId).second;
    LayerArrangement a = object()->layerArrangement();
    a.groupIds[from] = groupId;
    moveLayerBlock(a, from, from, from > top ? top + 1 : top);
    commitLayerArrangement(a, tr("Add Layer to Group"));
}

void LayerManager::removeLayerFromGroup(Layer* layer)
{
    const int from = getIndex(layer);
    if (from < 0 || layer->groupId() == 0) { return; }
    const QPair<int, int> range = object()->layerGroupRange(layer->groupId());
    LayerArrangement a = object()->layerArrangement();
    a.groupIds[from] = 0;
    if (from != range.first && from != range.second) { moveLayerBlock(a, from, from, range.second); }
    if (range.first == range.second)
    {
        a.groups.erase(std::remove_if(a.groups.begin(), a.groups.end(), [layer](const LayerGroup& g) { return g.id == layer->groupId(); }),
                       a.groups.end());
    }
    commitLayerArrangement(a, tr("Remove Layer from Group"));
}

void LayerManager::setLayerGroupCollapsed(int groupId, bool collapsed)
{
    LayerGroup* group = object()->findLayerGroup(groupId);
    if (group == nullptr || group->collapsed == collapsed) { return; }
    group->collapsed = collapsed;
    object()->modification();
    if (object()->isLayerFolded(currentLayerIndex())) { setCurrentLayer(object()->layerGroupRange(groupId).second); }
    emit editor()->updateTimeLine();
}

bool LayerManager::moveLayer(int from, int to)
{
    const int count = object()->getLayerCount();
    if (from < 0 || from >= count || to < 0 || to >= count || from == to) { return false; }
    const bool cameraAtBottom = object()->getLayer(0)->type() == Layer::CAMERA;
    const int floor = cameraAtBottom ? 1 : 0;
    const int groupId = object()->getLayer(from)->groupId();
    const LayerGroup* group = object()->findLayerGroup(groupId);
    const QPair<int, int> range = object()->layerGroupRange(groupId);
    const int lo = group ? range.first : from;
    const int hi = group ? range.second : from;
    if (from < floor || (group && group->collapsed && lo < floor)) { return false; }

    LayerArrangement a = object()->layerArrangement();
    if (group && !group->collapsed)
    {
        moveLayerBlock(a, from, from, qBound(std::max(lo, floor), to, hi));
    }
    else
    {
        if (to >= lo && to <= hi) { return false; }
        const bool up = to > from;
        QList<int> rest = a.groupIds;
        rest.erase(rest.begin() + lo, rest.begin() + hi + 1);
        int t = (to > hi ? to - (hi - lo + 1) : to) + (up ? 1 : 0);
        // Step past a group instead of splitting it
        while (t > 0 && t < rest.size() && rest[t - 1] != 0 && rest[t - 1] == rest[t]) { t += up ? 1 : -1; }
        moveLayerBlock(a, lo, hi, std::max(t, floor));
    }
    return commitLayerArrangement(a, tr("Move Layer"));
}

void LayerManager::setKeyTween(int frame, bool tween, CameraEasingType easing, const TweenState& state)
{
    Layer* layer = currentLayer();
    if (layer == nullptr || (layer->type() != Layer::BITMAP && layer->type() != Layer::VECTOR)) { return; }
    KeyFrame* key = layer->getKeyFrameAt(frame);
    if (key == nullptr) { return; }
    // The keyframe undo state captures the key at the current frame.
    if (editor()->currentFrame() != frame) { editor()->scrubTo(frame); }

    const SAVESTATE_ID saveStateId = editor()->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);
    key->setTween(tween);
    key->setTweenEasing(easing);
    key->setTweenState(state);
    editor()->undoRedo()->record(saveStateId, tr("Tween"));
    notifyLayerChanged(layer);
}

/**
 * @brief Get the length of current project
 * @return int: the position of the last key frame in the timeline + its length
 */
int LayerManager::animationLength(bool includeSounds)
{
    int maxFrame = -1;

    Object* o = object();
    for (int i = 0; i < o->getLayerCount(); i++)
    {
        if (o->getLayer(i)->type() == Layer::SOUND)
        {
            if (!includeSounds)
                continue;

            Layer* soundLayer = o->getLayer(i);
            soundLayer->foreachKeyFrame([&maxFrame](KeyFrame* keyFrame)
            {
                int endPosition = keyFrame->pos() + (keyFrame->length() - 1);
                if (endPosition > maxFrame)
                {
                    maxFrame = endPosition;
                }
            });
        }
        else
        {
            int lastFramePos = o->getLayer(i)->getMaxKeyFramePosition();
            if (lastFramePos > maxFrame)
            {
                maxFrame = lastFramePos;
            }
        }
    }
    return maxFrame;
}

void LayerManager::notifyAnimationLengthChanged()
{
    emit animationLengthChanged(animationLength(true));
}

void LayerManager::setExposureForSelectedFrames(int value, bool hold)
{
    UndoRedoManager* undoRedo = editor()->undoRedo();
    const QString description = hold ? tr("Set exposure") : value > 0 ? tr("Add exposure") : tr("Subtract exposure");
    const UndoMacro macro(undoRedo, description);
    for (int i = 0; i < count(); ++i)
    {
        Layer* layer = object()->getLayer(i);
        if (!layer->hasAnySelectedFrames()) { continue; }
        const SAVESTATE_ID saveStateId = undoRedo->createState(UndoRedoRecordType::KEYFRAME_MOVE, layer);
        layer->setExposureForSelectedFrames(value, hold);
        undoRedo->record(saveStateId, description);
    }
    notifyAnimationLengthChanged();
    emit editor()->framesModified();
}

static bool sameDrawing(Layer::LAYER_TYPE type, KeyFrame* a, KeyFrame* b)
{
    if (type == Layer::BITMAP)
    {
        auto* x = static_cast<BitmapImage*>(a);
        auto* y = static_cast<BitmapImage*>(b);
        // image() loads the frame before bounds() crops it.
        return *x->image() == *y->image() && x->bounds() == y->bounds();
    }
    QByteArray xData, yData;
    QXmlStreamWriter xWriter(&xData), yWriter(&yData);
    static_cast<VectorImage*>(a)->createDomElement(xWriter);
    static_cast<VectorImage*>(b)->createDomElement(yWriter);
    return xData == yData;
}

void LayerManager::cleanExposureOfSelectedFrames()
{
    UndoRedoManager* undoRedo = editor()->undoRedo();
    const QString description = tr("Clean exposure");
    const UndoMacro macro(undoRedo, description);
    for (int i = 0; i < count(); ++i)
    {
        Layer* layer = object()->getLayer(i);
        if (!layer->hasAnySelectedFrames() || !layer->isPaintable()) { continue; }

        QList<int> repeats;
        for (int pos : layer->selectedKeyFramesPositions())
        {
            const int previous = layer->getPreviousKeyFramePosition(pos);
            if (previous < pos && sameDrawing(layer->type(), layer->getKeyFrameAt(previous), layer->getKeyFrameAt(pos)))
            {
                repeats.append(pos);
            }
        }
        layer->deselectAll();
        if (repeats.isEmpty()) { continue; }

        // KEYFRAMES_REMOVE records the selected keyframes.
        for (int pos : repeats) { layer->setFrameSelected(pos, true); }
        const SAVESTATE_ID saveStateId = undoRedo->createState(UndoRedoRecordType::KEYFRAMES_REMOVE, layer);
        for (int pos : repeats) { layer->removeKeyFrame(pos); }
        undoRedo->record(saveStateId, description);
        notifyLayerChanged(layer);
    }
    notifyAnimationLengthChanged();
    emit editor()->framesModified();
}

void LayerManager::deselectAllFrames()
{
    for (int i = 0; i < count(); ++i) { object()->getLayer(i)->deselectAll(); }
}

void LayerManager::selectFrames(int fromLayer, int toLayer, int fromFrame, int toFrame)
{
    deselectAllFrames();
    const int lastLayer = qMin(qMax(fromLayer, toLayer), count() - 1);
    const int lastFrame = qMax(fromFrame, toFrame);
    for (int i = qMax(qMin(fromLayer, toLayer), 0); i <= lastLayer; ++i)
    {
        Layer* layer = object()->getLayer(i);
        for (int pos = qMin(fromFrame, toFrame); pos <= lastFrame; ++pos) { layer->setFrameSelected(pos, true); }
    }
}

// Moves every keyframe after `position` by `offset` in one pass and records the move. Keeps the frame selection.
static void shiftFramesAfter(UndoRedoManager* undoRedo, Layer* layer, int position, int offset, const QString& description)
{
    if (layer->getMaxKeyFramePosition() <= position) { return; }
    QList<QPair<int, int>> fromTo;
    layer->foreachKeyFrame([&](KeyFrame* key) {
        if (key->pos() > position) { fromTo.append(qMakePair(key->pos(), key->pos() + offset)); }
    });
    const QList<int> selected = layer->selectedKeyFramesByLast();
    const SAVESTATE_ID saveStateId = undoRedo->createState(UndoRedoRecordType::KEYFRAME_MOVE, layer);
    layer->moveKeyFrames(fromTo);
    undoRedo->record(saveStateId, description);
    // setFrameSelected puts each frame first in the by-last list, so restore from the oldest.
    for (auto it = selected.crbegin(); it != selected.crend(); ++it)
    {
        const int pos = *it > position ? *it + offset : *it;
        if (layer->keyExists(pos)) { layer->setFrameSelected(pos, true); }
    }
}

bool LayerManager::rippleInsertFrame()
{
    Layer* layer = currentLayer();
    if (!layer->visible()) { return false; }

    const int position = editor()->currentFrame();
    UndoRedoManager* undoRedo = editor()->undoRedo();
    const QString description = tr("Insert Frame");
    const UndoMacro macro(undoRedo, description);
    shiftFramesAfter(undoRedo, layer, position, 1, description);
    const bool added = editor()->addKeyFrame(currentLayerIndex(), position + 1) != nullptr;
    emit editor()->framesModified();
    return added;
}

bool LayerManager::rippleRemoveFrame()
{
    Layer* layer = currentLayer();
    const int position = editor()->currentFrame();
    const bool onKey = layer->keyExists(position);
    if (!layer->visible()) { return false; }
    if (onKey ? layer->keyFrameCount() == 1 : layer->getMaxKeyFramePosition() <= position) { return false; }

    UndoRedoManager* undoRedo = editor()->undoRedo();
    const QString description = tr("Delete Frame");
    const UndoMacro macro(undoRedo, description);
    if (onKey)
    {
        // The remove state records the keyframe at the current frame.
        const SAVESTATE_ID saveStateId = undoRedo->createState(UndoRedoRecordType::KEYFRAME_REMOVE, layer);
        layer->removeKeyFrame(position);
        undoRedo->record(saveStateId, description);
    }
    shiftFramesAfter(undoRedo, layer, position, -1, description);

    notifyAnimationLengthChanged();
    emit editor()->framesModified();
    return true;
}

bool LayerManager::moveSelectedFrames(int offset)
{
    QList<Layer*> selected;
    int firstFrame = INT_MAX;
    for (int i = 0; i < count(); ++i)
    {
        Layer* layer = object()->getLayer(i);
        if (!layer->hasAnySelectedFrames()) { continue; }
        selected.append(layer);
        firstFrame = qMin(firstFrame, layer->selectedKeyFramesPositions().first());
    }
    if (selected.isEmpty()) { return false; }

    offset = qMax(offset, 1 - firstFrame);
    if (offset == 0) { return false; }
    for (const Layer* layer : selected)
    {
        if (!layer->canMoveSelectedFramesToOffset(offset)) { return false; }
    }

    UndoRedoManager* undoRedo = editor()->undoRedo();
    const QString description = tr("Move Frames");
    const UndoMacro macro(undoRedo, description);
    for (Layer* layer : selected)
    {
        const SAVESTATE_ID saveStateId = undoRedo->createState(UndoRedoRecordType::KEYFRAME_MOVE, layer);
        layer->moveSelectedFrames(offset);
        undoRedo->record(saveStateId, description);
    }
    notifyAnimationLengthChanged();
    emit editor()->framesModified();
    return true;
}

int LayerManager::getIndex(Layer* layer) const
{
    const Object* o = object();
    for (int i = 0; i < o->getLayerCount(); ++i)
    {
        if (layer == o->getLayer(i))
            return i;
    }
    return -1;
}
