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
#include "attributes.h"
#include "editor.h"

#include "layersound.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "layercamera.h"
#include "undoredomanager.h"
#include "viewmanager.h"
#include "bitmapimage.h"
#include "vectorimage.h"
#include "cameraeasingtype.h"
#include "scribblearea.h"
#include "soundclip.h"
#include "soundmanager.h"

#include <algorithm>
#include <memory>

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
    mSoloVisibility.clear();
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
    Edit edit = editor()->edit(tr("Add Layer"));
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
    edit.recordLayerAdd(layer->id());
    edit.commit();

    return layer;
}

Layer* LayerManager::duplicateLayer(Layer* from, const QString& name)
{
    // Add Layer's undo takes the whole copy out.
    Edit edit = editor()->edit(tr("Add Layer"));
    Layer* to = createLayer(from->type(), name);
    if (to == nullptr) { return nullptr; }
    copyLayerAttributes(*from, *to);
    to->setName(name);
    if (from->groupId() != 0)
    {
        // A group is a run of adjacent layers: the copy goes right above the original. Add Layer's undo keeps this place.
        LayerArrangement a = object()->layerArrangement();
        for (QList<int>* list : { &a.layerIds, &a.groupIds }) { list->insert(getIndex(from) + 1, list->takeAt(getIndex(to))); }
        object()->setLayerArrangement(a);
        setCurrentLayer(to);
    }
    from->foreachKeyFrame([&](KeyFrame* key) {
        key = key->clone();
        to->addOrReplaceKeyFrame(key->pos(), key);
        if (to->type() == Layer::SOUND)
        {
            editor()->sound()->processSound(static_cast<SoundClip*>(key));
        }
    });
    if (!from->keyExists(1)) { to->removeKeyFrame(1); }
    edit.commit();
    return to;
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

    Edit edit = editor()->edit(tr("Delete Layer"));
    layer = takeLayer(index);
    edit.recordLayerRemove(layer, index);
    edit.commit();

    return Status::OK;
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
    // The undo stack keeps a taken layer alive: stop its clips.
    if (layer->type() == Layer::SOUND)
    {
        layer->foreachKeyFrame([](KeyFrame* key) { static_cast<SoundClip*>(key)->stop(); });
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

    setAttribute(layer, AttributeId::Name, newName);
    return Status::OK;
}

void LayerManager::toggleSolo(int index)
{
    Layer* solo = getLayer(index);
    if (solo == nullptr) { return; }
    Object* o = object();
    if (!mSoloVisibility.isEmpty() && mSoloLayerId == solo->id())
    {
        for (int i = 0; i < o->getLayerCount(); ++i)
        {
            Layer* layer = o->getLayer(i);
            layer->setVisible(mSoloVisibility.value(layer->id(), layer->visible()));
        }
        mSoloVisibility.clear();
    }
    else
    {
        if (mSoloVisibility.isEmpty())
        {
            for (int i = 0; i < o->getLayerCount(); ++i) { mSoloVisibility.insert(o->getLayer(i)->id(), o->getLayer(i)->visible()); }
        }
        // Sound and camera layers are not drawn: hiding them would mute the sound or the camera frame.
        for (int i = 0; i < o->getLayerCount(); ++i)
        {
            Layer* layer = o->getLayer(i);
            if (layer == solo || layer->isPaintable()) { layer->setVisible(layer == solo); }
        }
        mSoloLayerId = solo->id();
    }
    editor()->getScribbleArea()->onLayerChanged();
    emit editor()->updateTimeLine();
}

void LayerManager::notifyLayerChanged(Layer* layer)
{
    emit currentLayerChanged(getIndex(layer));
}

void LayerManager::setAttribute(Layer* layer, AttributeId id, const QVariant& value)
{
    const AttributeDescriptor& row = attribute(id);
    if (layer == nullptr || row.scope != AttributeScope::Layer || !row.appliesTo(layer->type())) { return; }
    const QVariant before = row.get(layer);
    if (before == value) { return; }
    Edit edit = editor()->edit(row.undoText(value));
    row.set(layer, nullptr, value);
    edit.recordAttribute(id, layer->id(), 0, before, value);
    notifyLayerChanged(layer);
    emit editor()->updateTimeLine();
    edit.commit();
}

bool LayerManager::setKeyAttribute(Layer* layer, int frame, AttributeId id, const QVariant& value)
{
    const AttributeDescriptor& row = attribute(id);
    if (layer == nullptr || row.scope != AttributeScope::Drawing || !row.appliesTo(layer->type())) { return false; }
    KeyFrame* key = layer->getLastKeyFrameAtPosition(frame);
    if (key == nullptr || row.get(layer, key) == value || editor()->refusesEdit(layer)) { return false; }
    const QVariant before = row.get(layer, key);
    Edit edit = editor()->edit(row.undoText(value));
    row.set(layer, key, value);
    edit.recordAttribute(id, layer->id(), key->pos(), before, value);
    emit editor()->frameModified(key->pos());
    edit.commit();
    return true;
}

void LayerManager::setBlendMode(Layer* layer, BlendMode mode)
{
    setAttribute(layer, AttributeId::BlendMode, static_cast<int>(mode));
}

void LayerManager::setRough(Layer* layer, bool rough)
{
    setAttribute(layer, AttributeId::Rough, rough);
}

void LayerManager::setTimesheetColumn(Layer* layer, TimesheetColumn column)
{
    setAttribute(layer, AttributeId::Timesheet, static_cast<int>(column));
}

bool LayerManager::setSceneLength(int length)
{
    const int old = object()->sceneLength();
    if (length < 0 || length > MaxFramesBound || length == old) { return false; }
    setProjectAttribute(AttributeId::SceneLength, length);
    return true;
}

void LayerManager::setProjectAttribute(AttributeId id, const QVariant& value)
{
    const AttributeDescriptor& row = attribute(id);
    if (row.scope != AttributeScope::Project) { return; }
    const QVariant before = row.get(*object());
    if (before == value) { return; }
    Edit edit = editor()->edit(row.undoText(value));
    row.set(*object(), value);
    edit.recordAttribute(id, 0, 0, before, value);
    notifyProjectChanged();
    edit.commit();
}

void LayerManager::notifyProjectChanged()
{
    notifyAnimationLengthChanged();
    emit editor()->framesModified();
    emit editor()->updateTimeLine();
}

void LayerManager::setCameraSize(LayerCamera* layer, QSize size)
{
    const QRect old = layer->getViewRect();
    const QRect rect(-size.width() / 2, -size.height() / 2, size.width(), size.height());
    if (rect == old) { return; }
    Edit edit = editor()->edit(tr("Camera Size"));
    layer->setViewRect(rect);
    edit.recordCameraSize(layer->id(), old, rect);
    editor()->view()->forceUpdateViewTransform();
    edit.commit();
}

void LayerManager::setStoryboard(Layer* layer, bool storyboard)
{
    setAttribute(layer, AttributeId::Storyboard, storyboard);
}

bool LayerManager::setPanelFields(Layer* layer, int frame, const PanelFields& fields)
{
    return setKeyAttribute(layer, frame, AttributeId::PanelFields, QVariant::fromValue(fields));
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
    Edit edit = editor()->edit(description);
    applyLayerArrangement(after);
    edit.recordLayerArrangement(before);
    edit.commit();
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

    Edit edit = editor()->edit(tr("Add Layer to Group"));
    if (layer->groupId() != 0) { removeLayerFromGroup(layer); }
    const int from = getIndex(layer);
    const int top = object()->layerGroupRange(groupId).second;
    LayerArrangement a = object()->layerArrangement();
    a.groupIds[from] = groupId;
    moveLayerBlock(a, from, from, from > top ? top + 1 : top);
    commitLayerArrangement(a, tr("Add Layer to Group"));
    edit.commit();
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

void LayerManager::setLayerGroupVisible(int groupId, bool visible)
{
    LayerArrangement a = object()->layerArrangement();
    for (LayerGroup& group : a.groups)
    {
        if (group.id == groupId) { group.visible = visible; }
    }
    commitLayerArrangement(a, visible ? tr("Show Layer Group") : tr("Hide Layer Group"));
}

void LayerManager::setLayerGroupOpacity(int groupId, qreal opacity)
{
    LayerArrangement a = object()->layerArrangement();
    for (LayerGroup& group : a.groups)
    {
        if (group.id == groupId) { group.opacity = qBound(0.0, opacity, 1.0); }
    }
    commitLayerArrangement(a, tr("Layer Group Opacity"));
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
    if (layer == nullptr || !layer->keyExists(frame)) { return; }
    if (setKeyAttribute(layer, frame, AttributeId::Tween, QVariant::fromValue(KeyTween{ tween, easing, state }))) { notifyLayerChanged(layer); }
}

void LayerManager::setKeyMark(int frame, Mark mark)
{
    Layer* layer = currentLayer();
    if (layer == nullptr || !attribute(AttributeId::Mark).appliesTo(layer->type())) { return; }
    if (layer->getLastKeyFrameAtPosition(frame) == nullptr)
    {
        if (!editor()->refusesEdit(layer)) { editor()->notice(tr("No Drawing to mark on frame %1").arg(frame)); }
        return;
    }
    if (setKeyAttribute(layer, frame, AttributeId::Mark, static_cast<int>(mark))) { notifyLayerChanged(layer); }
}

namespace
{
// Splits the longest segment of the curve until it has `count` vertices. A curve without segments stays.
void splitToVertexCount(VectorImage& image, int curve, int count)
{
    while (image.curve(curve).getVertexSize() > 0 && image.curve(curve).getVertexSize() < count)
    {
        const BezierCurve& c = image.curve(curve);
        int longest = 0;
        for (int i = 1; i < c.getVertexSize(); ++i)
        {
            if (QLineF(c.getVertex(i - 1), c.getVertex(i)).length() > QLineF(c.getVertex(longest - 1), c.getVertex(longest)).length())
            {
                longest = i;
            }
        }
        image.addPoint(curve, longest, 0.5);
    }
}

QPointF lerp(const QPointF& a, const QPointF& b, qreal t) { return a + (b - a) * t; }
}

Status LayerManager::createInbetweens(CameraEasingType easing)
{
    Layer* layer = currentLayer();
    if (layer == nullptr || editor()->refusesEdit(layer)) { return Status::FAIL; }
    auto refuse = [this](const QString& text) {
        editor()->notice(text);
        return Status(Status::FAIL, text);
    };
    if (layer->type() != Layer::VECTOR) { return refuse(tr("Create inbetweens works only on vector layers")); }
    QList<int> selected = layer->selectedKeyFramesPositions();
    if (selected.size() != 2) { return refuse(tr("Select two Drawings to create inbetweens")); }
    std::sort(selected.begin(), selected.end());
    const int from = selected[0];
    const int to = selected[1];

    auto vectorLayer = static_cast<LayerVector*>(layer);
    VectorImage* first = vectorLayer->getVectorImageAtFrame(from);
    VectorImage* last = vectorLayer->getVectorImageAtFrame(to);
    first->loadFile();
    last->loadFile();
    if (first->curveCount() != last->curveCount())
    {
        return refuse(tr("Cannot create inbetweens: the Drawings have %1 and %2 strokes").arg(first->curveCount()).arg(last->curveCount()));
    }
    QList<int> empty;
    for (int f = from + 1; f < to; ++f)
    {
        if (!layer->keyExists(f)) { empty.append(f); }
    }
    if (empty.isEmpty()) { return refuse(tr("No empty frames between the two Drawings")); }

    // Pair strokes by order; give each pair the same vertex count.
    std::unique_ptr<VectorImage> start(first->clone());
    std::unique_ptr<VectorImage> end(last->clone());
    for (int i = 0; i < start->curveCount(); ++i)
    {
        splitToVertexCount(*start, i, end->curve(i).getVertexSize());
        splitToVertexCount(*end, i, start->curve(i).getVertexSize());
    }
    start->deselectAll();

    Edit edit = editor()->edit(layer, tr("Create Inbetweens"));
    for (int f : empty)
    {
        const qreal t = easedProgress(easing, qreal(f - from) / (to - from));
        VectorImage* image = start->clone();
        for (int i = 0; i < image->curveCount(); ++i)
        {
            BezierCurve& c = image->curve(i);
            const BezierCurve& e = end->curve(i);
            c.setOrigin(lerp(c.getOrigin(), e.getOrigin(), t));
            for (int v = 0; v < std::min(c.getVertexSize(), e.getVertexSize()); ++v)
            {
                c.setC1(v, lerp(c.getC1(v), e.getC1(v), t));
                c.setC2(v, lerp(c.getC2(v), e.getC2(v), t));
                c.setVertex(v, lerp(c.getVertex(v), e.getVertex(v), t));
            }
            c.setWidth(c.getWidth() + (e.getWidth() - c.getWidth()) * t);
        }
        image->setTween(false);
        image->setMark(Mark::Inbetween);
        image->setLinkId(0);
        image->modification();
        layer->addKeyFrame(f, image);
    }
    edit.commit();
    notifyLayerChanged(layer);
    emit editor()->framesModified();
    return Status::OK;
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
    return qMax(maxFrame, o->sceneLength());
}

void LayerManager::notifyAnimationLengthChanged()
{
    emit animationLengthChanged(animationLength(true));
}

bool LayerManager::selectionRefusesEdit()
{
    for (int i = 0; i < count(); ++i)
    {
        const Layer* layer = object()->getLayer(i);
        if (layer->hasAnySelectedFrames() && editor()->refusesEdit(layer)) { return true; }
    }
    return false;
}

void LayerManager::setExposureForSelectedFrames(int value, bool hold)
{
    if (selectionRefusesEdit()) { return; }
    const QString description = hold ? tr("Set exposure") : value > 0 ? tr("Add exposure") : tr("Subtract exposure");
    Edit step = editor()->edit(description);
    for (int i = 0; i < count(); ++i)
    {
        Layer* layer = object()->getLayer(i);
        if (!layer->hasAnySelectedFrames()) { continue; }
        Edit edit = editor()->edit(layer, description);
        layer->setExposureForSelectedFrames(value, hold);
        edit.commit();
    }
    notifyAnimationLengthChanged();
    emit editor()->framesModified();
    step.commit();
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
    if (selectionRefusesEdit()) { return; }
    const QString description = tr("Clean exposure");
    Edit step = editor()->edit(description);
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

        Edit edit = editor()->edit(layer, description);
        for (int pos : repeats) { layer->removeKeyFrame(pos); }
        edit.commit();
        notifyLayerChanged(layer);
    }
    notifyAnimationLengthChanged();
    emit editor()->framesModified();
    step.commit();
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
static void shiftFramesAfter(LayerManager* layers, Editor* editor, Layer* layer, int position, int offset, const QString& description)
{
    if (layer->getMaxKeyFramePosition() <= position) { return; }
    QList<QPair<int, int>> fromTo;
    layer->foreachKeyFrame([&](KeyFrame* key) {
        if (key->pos() > position) { fromTo.append(qMakePair(key->pos(), key->pos() + offset)); }
    });
    const QList<int> selected = layer->selectedKeyFramesByLast();
    Edit edit = editor->edit(layer, description);
    LayerManager::moveKeyFrames(layer, fromTo);
    layers->moveSoundMarkers(layer, fromTo);
    edit.commit();
    // setFrameSelected puts each frame first in the by-last list, so restore from the oldest.
    for (auto it = selected.crbegin(); it != selected.crend(); ++it)
    {
        const int pos = *it > position ? *it + offset : *it;
        if (layer->keyExists(pos)) { LayerManager::setFrameSelected(layer, pos, true); }
    }
}

bool LayerManager::rippleInsertFrame()
{
    Layer* layer = currentLayer();
    if (editor()->refuses(layer)) { return false; }

    const int position = editor()->currentFrame();
    const QString description = tr("Insert Frame");
    Edit step = editor()->edit(description);
    shiftFramesAfter(this, editor(), layer, position, 1, description);
    const bool added = editor()->addKeyFrame(currentLayerIndex(), position + 1) != nullptr;
    emit editor()->framesModified();
    step.commit();
    return added;
}

bool LayerManager::rippleRemoveFrame()
{
    Layer* layer = currentLayer();
    const int position = editor()->currentFrame();
    const bool onKey = layer->keyExists(position);
    if (editor()->refuses(layer)) { return false; }
    if (onKey ? layer->keyFrameCount() == 1 : layer->getMaxKeyFramePosition() <= position) { return false; }

    const QString description = tr("Delete Frame");
    Edit step = editor()->edit(description);
    if (onKey)
    {
        Edit edit = editor()->edit(layer, description);
        layer->removeKeyFrame(position);
        edit.commit();
    }
    shiftFramesAfter(this, editor(), layer, position, -1, description);

    notifyAnimationLengthChanged();
    emit editor()->framesModified();
    step.commit();
    return true;
}

bool LayerManager::changeExposure(int frame, int delta)
{
    Layer* layer = currentLayer(0);
    if (layer == nullptr || editor()->refuses(layer)) { return false; }
    const KeyFrame* key = layer->getLastKeyFrameAtPosition(frame);
    if (key == nullptr) { return false; }
    const int position = key->pos();
    const int next = layer->getNextKeyFramePosition(position);
    if (next <= position) { return false; }
    const int length = next - position;
    const int shift = qMax(1, length + delta) - length;
    if (shift == 0) { return false; }

    const QString description = shift > 0 ? tr("Add exposure") : tr("Subtract exposure");
    shiftFramesAfter(this, editor(), layer, position, shift, description);
    notifyAnimationLengthChanged();
    emit editor()->framesModified();
    return true;
}

bool LayerManager::moveKeyFrame(int frame, int offset)
{
    Layer* layer = currentLayer();
    if (layer == nullptr || editor()->refuses(layer)) { return false; }
    Edit edit = editor()->edit(layer, tr("Move Frame"));
    const bool moved = layer->moveKeyFrame(frame, offset);
    if (moved) { edit.commit(); }
    notifyAnimationLengthChanged();
    emit editor()->framesModified();
    return moved;
}

bool LayerManager::linkDrawing(int from, int to)
{
    Layer* layer = currentLayer(0);
    if (layer == nullptr || !layer->visible() || !layer->isPaintable() || to < 1 || layer->keyExists(to) ||
        editor()->refusesEdit(layer)) { return false; }
    KeyFrame* source = layer->getLastKeyFrameAtPosition(from);
    if (source == nullptr) { return false; }
    // A link id left on a key with no copy is harmless, so undo need not clear it.
    if (source->linkId() == 0) { source->setLinkId(layer->newLinkId()); }

    Edit edit = editor()->edit(layer, tr("Link Drawing"));
    layer->addKeyFrame(to, source->clone());
    notifyAnimationLengthChanged();
    emit editor()->frameModified(to);
    edit.commit();
    return true;
}

bool LayerManager::unlinkDrawing(int frame)
{
    Layer* layer = currentLayer(0);
    if (layer == nullptr) { return false; }
    KeyFrame* key = layer->getLastKeyFrameAtPosition(frame);
    if (!layer->isLinked(key) || !setKeyAttribute(layer, frame, AttributeId::LinkedTo, 0)) { return false; }
    key->modification();
    return true;
}

void LayerManager::syncLinkedDrawings(Layer* layer, int frame)
{
    if (layer == nullptr) { return; }
    KeyFrame* key = layer->getLastKeyFrameAtPosition(frame);
    for (KeyFrame* copy : layer->linkedKeys(key))
    {
        copy->copyImageFrom(key);
        emit editor()->frameModified(copy->pos());
    }
}

void LayerManager::moveSoundMarkers(const Layer* layer, const QList<QPair<int, int>>& fromTo)
{
    if (layer->type() != Layer::SOUND || fromTo.isEmpty()) { return; }
    ObjectData* data = object()->data();
    QMap<int, FrameMarker> markers = data->markers(layer->id());
    // Take every moving marker out first, so a marker moved into another clip's old span moves only once.
    QMap<int, FrameMarker> moved;
    for (const QPair<int, int>& move : fromTo)
    {
        const KeyFrame* clip = layer->getKeyFrameAt(move.second);
        const int end = move.first + (clip ? clip->length() : 1);
        for (auto it = markers.lowerBound(move.first); it != markers.end() && it.key() < end;)
        {
            moved.insert(it.key() + move.second - move.first, it.value());
            it = markers.erase(it);
        }
    }
    markers.insert(moved);
    data->setMarkers(layer->id(), markers);
}

void LayerManager::setMarker(int layerId, int frame, const FrameMarker& marker)
{
    ObjectData* data = object()->data();
    const QMap<int, FrameMarker> before = data->markers(layerId);
    data->setMarker(frame, marker, layerId);
    markersChanged(layerId, before, before.contains(frame) ? tr("Edit Marker") : tr("Add Marker"));
}

bool LayerManager::moveMarker(int layerId, int from, int to)
{
    ObjectData* data = object()->data();
    const QMap<int, FrameMarker> before = data->markers(layerId);
    if (!before.contains(from) || from == to || to < 1) { return false; }
    data->removeMarker(from, layerId);
    data->setMarker(to, before.value(from), layerId);
    markersChanged(layerId, before, tr("Move Marker"));
    return true;
}

void LayerManager::removeMarker(int layerId, int frame)
{
    ObjectData* data = object()->data();
    const QMap<int, FrameMarker> before = data->markers(layerId);
    if (!before.contains(frame)) { return; }
    data->removeMarker(frame, layerId);
    markersChanged(layerId, before, tr("Remove Marker"));
}

void LayerManager::markersChanged(int layerId, const QMap<int, FrameMarker>& before, const QString& description)
{
    Edit edit = editor()->edit(description);
    edit.recordMarkers(layerId, before);
    emit editor()->updateTimeLine();
    edit.commit();
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
    if (selected.isEmpty() || selectionRefusesEdit()) { return false; }

    offset = qMax(offset, 1 - firstFrame);
    if (offset == 0) { return false; }
    for (const Layer* layer : selected)
    {
        if (!layer->canMoveSelectedFramesToOffset(offset)) { return false; }
    }

    const QString description = tr("Move Frames");
    Edit step = editor()->edit(description);
    for (Layer* layer : selected)
    {
        QList<QPair<int, int>> fromTo;
        for (int pos : layer->selectedKeyFramesPositions()) { fromTo.append(qMakePair(pos, pos + offset)); }
        Edit edit = editor()->edit(layer, description);
        layer->moveSelectedFrames(offset);
        moveSoundMarkers(layer, fromTo);
        edit.commit();
    }
    notifyAnimationLengthChanged();
    emit editor()->framesModified();
    step.commit();
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
