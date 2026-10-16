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
#include "layer.h"

#include <QApplication>
#include <QDebug>
#include <QSettings>
#include <QPainter>
#include <QDomElement>
#include <QSet>
#include "keyframe.h"
#include "util.h"

// Used to sort the selected frames list
bool sortAsc(int left, int right)
{
    return left < right;
}

Layer::Layer(int id, LAYER_TYPE eType)
{
    Q_ASSERT(eType != UNDEFINED);

    meType = eType;
    mName = QString(tr("Undefined Layer"));

    mId = id;
}

Layer::~Layer()
{
    for (auto it : mKeyFrames)
    {
        KeyFrame* pKeyFrame = it.second;
        delete pKeyFrame;
    }
    mKeyFrames.clear();
}

void Layer::foreachKeyFrame(std::function<void(KeyFrame*)> action) const
{
    for (auto pair : mKeyFrames)
    {
        action(pair.second);
    }
}

bool Layer::keyExists(int position) const
{
    return (mKeyFrames.find(position) != mKeyFrames.end());
}

KeyFrame* Layer::getKeyFrameAt(int position) const
{
    auto it = mKeyFrames.find(position);
    if (it == mKeyFrames.end())
    {
        return nullptr;
    }
    return it->second;
}

KeyFrame* Layer::getLastKeyFrameAtPosition(int position) const
{
    if (position < 1)
    {
        position = 1;
    }
    auto it = mKeyFrames.lower_bound(position);
    if (it == mKeyFrames.end())
    {
        return nullptr;
    }
    return it->second;
}

int Layer::getPreviousKeyFramePosition(int position) const
{
    auto it = mKeyFrames.upper_bound(position);
    if (it == mKeyFrames.end())
    {
        return firstKeyFramePosition();
    }
    return it->first;
}

int Layer::getNextKeyFramePosition(int position) const
{
    // workaround: bug with lower_bound?
    // when position is before the first frame it == mKeyFrames.end() for some reason
    if (position < firstKeyFramePosition())
    {
       return firstKeyFramePosition();
    }

    auto it = mKeyFrames.lower_bound(position);
    if (it == mKeyFrames.end())
    {
        return getMaxKeyFramePosition();
    }

    if (it != mKeyFrames.begin())
    {
        --it;
    }
    return it->first;
}

int Layer::getPreviousFrameNumber(int position, bool isAbsolute) const
{
    int prevNumber;

    if (isAbsolute)
        prevNumber = getPreviousKeyFramePosition(position);
    else
        prevNumber = position - 1;

    if (prevNumber >= position)
    {
        return -1; // There is no previous keyframe
    }
    return prevNumber;
}

int Layer::getNextFrameNumber(int position, bool isAbsolute) const
{
    int nextNumber;

    if (isAbsolute)
        nextNumber = getNextKeyFramePosition(position);
    else
        nextNumber = position + 1;

    if (nextNumber <= position)
        return -1; // There is no next keyframe

    return nextNumber;
}

int Layer::firstKeyFramePosition() const
{
    if (!mKeyFrames.empty())
    {
        return mKeyFrames.rbegin()->first; // rbegin is the lowest key frame position
    }
    return 0;
}

int Layer::getMaxKeyFramePosition() const
{
    if (!mKeyFrames.empty())
    {
        return mKeyFrames.begin()->first; // begin is the highest key frame position
    }
    return 0;
}

bool Layer::addNewKeyFrameAt(int position)
{
    if (position <= 0) return false;

    KeyFrame* key = createKeyFrame(position);
    if (!addKeyFrame(position, key))
    {
        delete key;
        return false;
    }
    return true;
}

void Layer::addOrReplaceKeyFrame(int position, KeyFrame* pKeyFrame)
{
    Q_ASSERT(position > 0);
    pKeyFrame->setPos(position);
    loadKey(pKeyFrame);
    markFrameAsDirty(position);
}

bool Layer::addKeyFrame(int position, KeyFrame* pKeyFrame)
{
    if (keyExists(position))
    {
        return false;
    }

    pKeyFrame->setPos(position);
    mKeyFrames.emplace(position, pKeyFrame);
    markFrameAsDirty(position);
    return true;
}

bool Layer::insertExposureAt(int position)
{
    if(position < 1 || position > getMaxKeyFramePosition() || !getKeyFrameAt(position))
    {
        return false;
    }

    newSelectionOfConnectedFrames(position + 1);
    moveSelectedFrames(1);
    deselectAll();

    return true;
}

bool Layer::removeKeyFrame(int position)
{
    if (keyFrameCount() == 1 && this->type() != SOUND) { return false; }
    auto frame = getKeyFrameWhichCovers(position);

    if (frame)
    {
        removeFromSelectionList(frame->pos());
        mKeyFrames.erase(frame->pos());
        markFrameAsDirty(frame->pos());
        delete frame;
    }
    return true;
}

void Layer::removeFromSelectionList(int position)
{
    mSelectedFrames_byLast.removeAll(position);
    mSelectedFrames_byPosition.removeAll(position);
}

bool Layer::moveKeyFrame(int position, int offset)
{
    int newPos = position + offset;
    if (newPos < 1) { return false; }

    auto listOfFramesLast = mSelectedFrames_byLast;
    auto listOfFramesPos = mSelectedFrames_byPosition;
    bool frameSelected = isFrameSelected(position);

    if (swapKeyFrames(position, newPos)) {

        auto oldPosIndex = mSelectedFrames_byPosition.indexOf(position);
        auto newPosIndex = mSelectedFrames_byPosition.indexOf(newPos);

        auto oldLastIndex = mSelectedFrames_byLast.indexOf(position);
        auto newLastndex = mSelectedFrames_byLast.indexOf(newPos);


        // Old position is selected
        if (oldPosIndex != -1) {
            mSelectedFrames_byPosition[oldPosIndex] = newPos;
            mSelectedFrames_byLast[oldLastIndex] = newPos;
        }

        // Old position is selected
        if (newPosIndex != -1) {
            mSelectedFrames_byPosition[newPosIndex] = position;
            mSelectedFrames_byLast[newLastndex] = position;
        }

        // Keep the list sorted, moveSelectedFrames() relies on it
        std::sort(mSelectedFrames_byPosition.begin(), mSelectedFrames_byPosition.end(), sortAsc);
        return true;
    }

    mSelectedFrames_byLast.clear();
    mSelectedFrames_byPosition.clear();

    setFrameSelected(position, true);

    // If the move fails, assume we can't move at all and revert to old selection
    if (!moveSelectedFrames(offset)) {
        mSelectedFrames_byLast = listOfFramesLast;
        mSelectedFrames_byPosition = listOfFramesPos;
        return false;
    }

    // Remove old position from selection list
    listOfFramesLast.removeOne(position);
    listOfFramesPos.removeOne(position);

    mSelectedFrames_byLast = listOfFramesLast;
    mSelectedFrames_byPosition = listOfFramesPos;

    // If the frame was selected prior to moving, make sure it's still selected.
    setFrameSelected(newPos, frameSelected);

    return true;
}

// Current behaviour, need to refresh the swapped cels
bool Layer::swapKeyFrames(int position1, int position2)
{
    KeyFrame* pFirstFrame = nullptr;
    KeyFrame* pSecondFrame = nullptr;

    if (mKeyFrames.count(position1) != 1 || mKeyFrames.count(position2) != 1)
    {
        return false;
    }

    // Both keys exist
    pFirstFrame = mKeyFrames[position1];
    pSecondFrame = mKeyFrames[position2];

    mKeyFrames[position1] = pSecondFrame;
    mKeyFrames[position2] = pFirstFrame;

    pFirstFrame->setPos(position2);
    pSecondFrame->setPos(position1);

    pFirstFrame->modification();
    pSecondFrame->modification();

    markFrameAsDirty(position1);
    markFrameAsDirty(position2);

    return true;
}

bool Layer::loadKey(KeyFrame* pKey)
{
    auto it = mKeyFrames.find(pKey->pos());
    if (it != mKeyFrames.end())
    {
        delete it->second;
        mKeyFrames.erase(it);
    }
    mKeyFrames.emplace(pKey->pos(), pKey);
    return true;
}

Status Layer::save(const QString& sDataFolder, QStringList& attachedFiles, ProgressCallback progressStep)
{
    DebugDetails dd;
    dd << "\n[Layer SAVE diagnostics]\n";

    bool ok = true;

    std::vector<KeyFrame*> keys;
    for (auto pair : mKeyFrames)
        keys.push_back(pair.second);
    std::vector<Status> results(keys.size(), Status::OK);

    // Bitmap keys are independent PNG files: encode them in parallel, a chunk at a time for progress.
    const int count = static_cast<int>(keys.size());
    const int chunk = (type() == BITMAP) ? parallelWorkerCount() * 4 : 1;
    for (int start = 0; start < count; start += chunk)
    {
        const int end = std::min(start + chunk, count);
        parallelFor(end - start, [&](int i) { results[start + i] = saveKeyFrameFile(keys[start + i], sDataFolder); });

        for (int i = start; i < end; ++i)
        {
            KeyFrame* keyFrame = keys[i];
            if (results[i].ok())
            {
                if (!keyFrame->fileName().isEmpty())
                    attachedFiles.append(keyFrame->fileName());
            }
            else
            {
                ok = false;
                dd.collect(results[i].details());
                dd << QString("- Keyframe[%1] failed to save").arg(keyFrame->pos());
            }
            progressStep();
        }
    }
    if (!ok)
    {
        dd << "\nError: Failed to save one or more files";
        return Status(Status::FAIL, dd);
    }
    return Status::OK;
}

void Layer::setModified(int position, bool modified) const
{
    KeyFrame* key = getKeyFrameAt(position);
    if (key)
    {
        key->setModified(modified);
    }
}

bool Layer::isFrameSelected(int position) const
{
    KeyFrame* keyFrame = getKeyFrameWhichCovers(position);
    if (keyFrame == nullptr) { return false; }

    return mSelectedFrames_byLast.contains(keyFrame->pos());
}

void Layer::setFrameSelected(int position, bool isSelected)
{
    KeyFrame* keyFrame = getKeyFrameWhichCovers(position);
    if (keyFrame != nullptr)
    {
        int startPosition = keyFrame->pos();

        if (isSelected && !mSelectedFrames_byLast.contains(startPosition))
        {
            // Add the selected frame to the lists
            mSelectedFrames_byLast.insert(0, startPosition);
            mSelectedFrames_byPosition.append(startPosition);

            // We need to keep the list of selected frames sorted
            // in order to easily handle their movement
            std::sort(mSelectedFrames_byPosition.begin(), mSelectedFrames_byPosition.end(), sortAsc);
        }
        else if (!isSelected)
        {
            mSelectedFrames_byLast.removeOne(startPosition);
            mSelectedFrames_byPosition.removeOne(startPosition);
        }
    }
}

void Layer::toggleFrameSelected(int position, bool allowMultiple)
{
    bool wasSelected = isFrameSelected(position);

    if (!allowMultiple)
    {
        deselectAll();
    }

    setFrameSelected(position, !wasSelected);
}

void Layer::extendSelectionTo(int position)
{
    if (mSelectedFrames_byLast.count() > 0)
    {
        int lastSelected = mSelectedFrames_byLast[0];
        int startPos;
        int endPos;

        if (lastSelected < position)
        {
            startPos = lastSelected;
            endPos = position;
        }
        else
        {
            startPos = position;
            endPos = lastSelected;
        }

        int i = startPos;
        while (i <= endPos)
        {
            setFrameSelected(i, true);
            i++;
        }
    }
}

bool Layer::newSelectionOfConnectedFrames(int position)
{
    // Deselect all before extending to make sure we don't get already
    // selected frames
    deselectAll();

    if (!keyExists(position)) { return false; }

    setFrameSelected(position, true);

    // Find keyframes that are connected and make sure we're below max.
    while (position < getMaxKeyFramePosition() && getKeyFrameWhichCovers(position) != nullptr) {
        position++;
    }

    extendSelectionTo(position);

    return true;
}

void Layer::selectAllFramesAfter(int position)
{
    int startPosition = position;
    int endPosition = getMaxKeyFramePosition();

    if (!keyExists(startPosition))
    {
        startPosition = getNextKeyFramePosition(startPosition);
    }

    if (startPosition > 0 && startPosition <= endPosition)
    {
        deselectAll();
        setFrameSelected(startPosition, true);
        extendSelectionTo(endPosition);
    }
}

void Layer::deselectAll()
{
    mSelectedFrames_byLast.clear();
    mSelectedFrames_byPosition.clear();
}

bool Layer::canMoveSelectedFramesToOffset(int offset) const
{
    QList<int> newByPositions = mSelectedFrames_byPosition;

    for (int pos : newByPositions)
    {
        pos += offset;
        if (keyExists(pos) && !newByPositions.contains(pos)) {
            return false;
        }
    }

    return true;
}

void Layer::setExposureForSelectedFrames(int value, bool hold)
{
    if (mSelectedFrames_byPosition.isEmpty()) { return; }

    // One pass from the first keyframe: each selected hold changes and shifts every later keyframe.
    QHash<int, int> newPositions;
    for (int pos : mSelectedFrames_byPosition) { newPositions.insert(pos, pos); }
    QList<QPair<int, int>> fromTo;
    int shift = 0;
    int previous = 0;
    bool previousSelected = false;
    for (auto it = mKeyFrames.crbegin(); it != mKeyFrames.crend(); ++it)
    {
        const int pos = it->first;
        if (previousSelected)
        {
            const int length = pos - previous;
            shift += qMax(1, hold ? value : length + value) - length;
        }
        if (shift != 0) { fromTo.append(qMakePair(pos, pos + shift)); }
        previousSelected = newPositions.contains(pos);
        if (previousSelected) { newPositions[pos] = pos + shift; }
        previous = pos;
    }

    QList<int> byLast = mSelectedFrames_byLast;
    QList<int> byPosition = mSelectedFrames_byPosition;
    moveKeyFrames(fromTo);
    for (int& pos : byLast) { pos = newPositions.value(pos); }
    for (int& pos : byPosition) { pos = newPositions.value(pos); }
    mSelectedFrames_byLast = byLast;
    mSelectedFrames_byPosition = byPosition;
}

bool Layer::reverseOrderOfSelection()
{
    QList<int> selectedIndexes = mSelectedFrames_byPosition;

    if (selectedIndexes.isEmpty()) { return false; }

    for (int swapBegin = 0, swapEnd = selectedIndexes.count()-1; swapBegin < swapEnd; swapBegin++, swapEnd--) {
        int oldPos = selectedIndexes[swapBegin];
        int newPos = selectedIndexes[swapEnd];
        bool canSwap = swapKeyFrames(oldPos, newPos);
        Q_ASSERT(canSwap);
    }
    return true;
}

bool Layer::moveSelectedFrames(int offset)
{
    if (offset == 0 || mSelectedFrames_byPosition.count() <= 0) {
        return false;
    }

    // If we are moving to the right we start moving selected frames from the highest (right) to the lowest (left)
    int indexInSelection = mSelectedFrames_byPosition.count() - 1;
    int step = -1;

    if (offset < 0)
    {
        // If we are moving to the left we start moving selected frames from the lowest (left) to the highest (right)
        indexInSelection = 0;
        step = 1;

        // Check if we are not moving out of the timeline
        if (mSelectedFrames_byPosition[0] + offset < 1) {
            offset = 1 - mSelectedFrames_byPosition[0];
        }
    }

    while (!canMoveSelectedFramesToOffset(offset)) { offset += 1; }
    if (offset == 0) { return false; }

    for (; indexInSelection > -1 && indexInSelection < mSelectedFrames_byPosition.count(); indexInSelection += step)
    {
        int fromPos = mSelectedFrames_byPosition[indexInSelection];
        int toPos = fromPos + offset;

        // Get the frame to move
        KeyFrame* selectedFrame = getKeyFrameAt(fromPos);

        Q_ASSERT(!keyExists(toPos));

        mKeyFrames.erase(fromPos);
        markFrameAsDirty(fromPos);

        // Update the position of the selected frame
        selectedFrame->setPos(toPos);
        mKeyFrames.insert(std::make_pair(toPos, selectedFrame));
        markFrameAsDirty(toPos);
    }

    // Update selection lists
    for (int& pos : mSelectedFrames_byPosition)
    {
        pos += offset;
    }
    for (int& pos : mSelectedFrames_byLast)
    {
        pos += offset;
    }
    return true;
}

bool Layer::moveKeyFrames(const QList<QPair<int, int>>& fromTo)
{
    QSet<int> sources;
    QSet<int> targets;
    for (const QPair<int, int>& move : fromTo)
    {
        sources.insert(move.first);
        targets.insert(move.second);
    }
    if (sources.count() != fromTo.count() || targets.count() != fromTo.count()) { return false; }
    for (const QPair<int, int>& move : fromTo)
    {
        if (!keyExists(move.first) || (keyExists(move.second) && !sources.contains(move.second))) { return false; }
    }

    deselectAll();

    // Take all keyframes out first: a target can be the source of another move.
    QList<KeyFrame*> keys;
    for (const QPair<int, int>& move : fromTo)
    {
        keys << mKeyFrames[move.first];
        mKeyFrames.erase(move.first);
        markFrameAsDirty(move.first);
    }
    for (int i = 0; i < fromTo.count(); i++)
    {
        const int toPos = fromTo[i].second;
        keys[i]->setPos(toPos);
        keys[i]->modification();
        mKeyFrames.emplace(toPos, keys[i]);
        markFrameAsDirty(toPos);
    }
    return true;
}

bool Layer::isPaintable() const
{
    return (type() == BITMAP || type() == VECTOR);
}

bool Layer::keyExistsWhichCovers(int frameNumber)
{
    return getKeyFrameWhichCovers(frameNumber) != nullptr;
}

KeyFrame* Layer::getKeyFrameWhichCovers(int frameNumber) const
{
    auto keyFrame = getLastKeyFrameAtPosition(frameNumber);
    if (keyFrame != nullptr)
    {
        if (keyFrame->pos() + keyFrame->length() > frameNumber)
        {
            return keyFrame;
        }
    }
    return nullptr;
}

QDomElement Layer::createBaseDomElement(QDomDocument& doc) const
{
    QDomElement layerTag = doc.createElement("layer");
    layerTag.setAttribute("id", id());
    layerTag.setAttribute("name", name());
    layerTag.setAttribute("visibility", visible());
    layerTag.setAttribute("type", type());
    // Optional: only written when off, so older versions still read the file.
    if (!onionSkin()) { layerTag.setAttribute("onion", 0); }
    if (blendMode() != BlendMode::Normal) { layerTag.setAttribute("blend", static_cast<int>(blendMode())); }
    return layerTag;
}

void Layer::loadBaseDomElement(const QDomElement& elem)
{
    if (!elem.attribute("id").isNull())
    {
        int id = elem.attribute("id").toInt();
        setId(id);
    }
    setName(elem.attribute("name", "untitled"));
    setVisible(elem.attribute("visibility", "1").toInt());
    setOnionSkin(elem.attribute("onion", "1").toInt());
    const int blend = elem.attribute("blend", "0").toInt();
    setBlendMode(blend >= 0 && blend <= static_cast<int>(BlendMode::Add) ? static_cast<BlendMode>(blend) : BlendMode::Normal);
}

TweenState Layer::tweenStateAt(int frame) const
{
    const KeyFrame* key = getLastKeyFrameAtPosition(frame);
    if (key == nullptr) { return TweenState(); }
    const TweenState a = key->tweenState();
    const int next = getNextKeyFramePosition(key->pos());
    if (!key->tween() || frame == key->pos() || next <= frame) { return a; }

    const TweenState b = getKeyFrameAt(next)->tweenState();
    const qreal t = easedProgress(key->tweenEasing(), qreal(frame - key->pos()) / (next - key->pos()));
    auto lerp = [t](qreal from, qreal to) { return from + (to - from) * t; };
    return TweenState{ lerp(a.tx, b.tx), lerp(a.ty, b.ty), lerp(a.rot, b.rot), lerp(a.scale, b.scale), lerp(a.opacity, b.opacity) };
}

void Layer::writeKeyTween(QDomElement& image, const KeyFrame* key)
{
    // Optional: only written when set, so older versions still read the file.
    if (key->tween())
    {
        image.setAttribute("tween", 1);
        image.setAttribute("easing", static_cast<int>(key->tweenEasing()));
    }
    const TweenState s = key->tweenState();
    if (s.tx != 0) { image.setAttribute("tx", s.tx); }
    if (s.ty != 0) { image.setAttribute("ty", s.ty); }
    if (s.rot != 0) { image.setAttribute("rot", s.rot); }
    if (s.scale != 1) { image.setAttribute("scale", s.scale); }
}

void Layer::readKeyTween(const QDomElement& image, KeyFrame* key)
{
    if (key == nullptr) { return; }
    // Untrusted file: an out-of-range or non-finite value loads as the default.
    auto number = [&image](const char* name, qreal fallback) {
        bool ok = false;
        const qreal v = image.attribute(name).toDouble(&ok);
        return ok && qIsFinite(v) ? v : fallback;
    };
    key->setTween(image.attribute("tween", "0").toInt() == 1);
    const int easing = image.attribute("easing", "0").toInt();
    const bool validEasing = easing >= 0 && easing <= static_cast<int>(CameraEasingType::OUTINBOUNCE);
    key->setTweenEasing(validEasing ? static_cast<CameraEasingType>(easing) : CameraEasingType::LINEAR);
    key->setTweenState(TweenState{ number("tx", 0), number("ty", 0), number("rot", 0), number("scale", 1),
                                   qBound(0.0, number("opacity", 1), 1.0) });
}
