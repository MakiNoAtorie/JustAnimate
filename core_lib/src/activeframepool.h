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

#ifndef ACTIVEFRAMEPOOL_H
#define ACTIVEFRAMEPOOL_H

#include <list>
#include <unordered_map>
#include <unordered_set>
#include "keyframe.h"


/**
 * ActiveFramePool keeps the loaded Drawings in a LRU list under a memory budget.
 * It listens to every key of the object's layers, so each load enters the pool, also loads it did not start.
 * Object::updateActiveFrames() updates it on every scrub.
 *
 * Note: ActiveFramePool does not handle file saving. It loads frames, but never writes frames to disks.
 */
class ActiveFramePool : public KeyFrameEventListener
{
public:
    explicit ActiveFramePool();
    virtual ~ActiveFramePool();

    /** Pools the key: loads it now, or with `now` false starts a background load */
    void put(KeyFrame* key, bool now);
    /** Listens to the key, so its loads enter the pool */
    void watch(KeyFrame* key);
    void clear();
    void resize(quint64 memoryBudget);
    void setMinFrameCount(size_t frameCount);
    /** Sets the playhead and unloads pooled frames last used more than `distance` frames from it, whatever the budget */
    void unloadFramesFarFrom(int frame, int distance);
    quint64 usedMemory() const { return mTotalUsedMemory; }

    void onKeyFrameDestroy(KeyFrame*) override;
    void onKeyFrameLoaded(KeyFrame*) override;

private:
    struct Entry
    {
        std::list<KeyFrame*>::iterator it;
        quint64 bytes = 0;
        int usedAt = 0; // the playhead when the key was last put or loaded
    };

    void touch(KeyFrame* key);
    void forget(KeyFrame* key);
    void discardLeastUsedFrames();

    std::list<KeyFrame*> mCacheFramesList;
    std::unordered_map<KeyFrame*, Entry> mCacheFramesMap;
    std::unordered_set<KeyFrame*> mWatched;
    quint64 mMemoryBudgetInBytes = 1024 * 1024 * 1024; // 1GB
    quint64 mTotalUsedMemory = 0;
    size_t mMinFrameCount = 15;
    int mPlayhead = 1;
};

#endif // ACTIVEFRAMEPOOL_H
