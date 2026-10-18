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

#include "activeframepool.h"
#include <cstdlib>
#include "keyframe.h"


ActiveFramePool::ActiveFramePool()
{
    Q_ASSERT(mMemoryBudgetInBytes >= (1024 * 1024 * 100)); // at least 100MB
}

ActiveFramePool::~ActiveFramePool()
{
    clear();
}

void ActiveFramePool::put(KeyFrame* key, bool now)
{
    if (key == nullptr)
        return;

    Q_ASSERT(key->pos() > 0);

    watch(key);
    if (now)
        key->loadFile();
    else
        key->loadFileAsync();
    touch(key);
    discardLeastUsedFrames();
}

void ActiveFramePool::watch(KeyFrame* key)
{
    if (mWatched.insert(key).second)
        key->addEventListener(this);
}

void ActiveFramePool::clear()
{
    for (KeyFrame* key : mWatched)
    {
        key->removeEventListner(this);
    }
    mWatched.clear();
    mCacheFramesList.clear();
    mCacheFramesMap.clear();
    mTotalUsedMemory = 0;
}

void ActiveFramePool::resize(quint64 memoryBudget)
{
    memoryBudget = qMin(memoryBudget, quint64(1024) * 1024 * 1024 * 16); // 16GB
    memoryBudget = qMax(memoryBudget, quint64(1024) * 1024 * 100); // 100MB
    mMemoryBudgetInBytes = memoryBudget;
    discardLeastUsedFrames();
}

void ActiveFramePool::setMinFrameCount(size_t frameCount)
{
    mMinFrameCount = frameCount;
}

void ActiveFramePool::unloadFramesFarFrom(int frame, int distance)
{
    mPlayhead = frame;
    for (auto it = mCacheFramesList.begin(); it != mCacheFramesList.end();)
    {
        KeyFrame* key = *it++;
        if (std::abs(mCacheFramesMap[key].usedAt - frame) > distance)
        {
            key->unloadFile(); // keeps frames with unsaved changes loaded
            forget(key);
        }
    }
}

void ActiveFramePool::onKeyFrameDestroy(KeyFrame* key)
{
    mWatched.erase(key);
    forget(key); // the stored size: memoryUsage() is not safe in the KeyFrame's destructor
}

void ActiveFramePool::onKeyFrameLoaded(KeyFrame* key)
{
    touch(key);
}

void ActiveFramePool::touch(KeyFrame* key)
{
    forget(key);
    mCacheFramesList.push_front(key);
    Entry& e = mCacheFramesMap[key];
    e.it = mCacheFramesList.begin();
    e.bytes = key->memoryUsage();
    e.usedAt = mPlayhead;
    mTotalUsedMemory += e.bytes;
}

void ActiveFramePool::forget(KeyFrame* key)
{
    auto found = mCacheFramesMap.find(key);
    if (found == mCacheFramesMap.end())
        return;
    mTotalUsedMemory -= found->second.bytes;
    mCacheFramesList.erase(found->second.it);
    mCacheFramesMap.erase(found);
}

void ActiveFramePool::discardLeastUsedFrames()
{
    while ((mTotalUsedMemory > mMemoryBudgetInBytes) && (mCacheFramesList.size() > mMinFrameCount))
    {
        KeyFrame* last = mCacheFramesList.back();
        last->unloadFile();
        forget(last);
    }
}
