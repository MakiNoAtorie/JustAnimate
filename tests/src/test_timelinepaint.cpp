/*

JustAnimate - Traditional Animation Software

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "catch.hpp"

#include <algorithm>
#include <QImage>

#include "editor.h"
#include "keyframe.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "stageprofiler.h"
#include "testfixture.h"
#include "timeline.h"
#include "timelinecells.h"

namespace
{

// Turns the stage profiler on for one test and restores it.
struct ProfilerOn
{
    const bool was = StageProfiler::enabled();
    ProfilerOn() { StageProfiler::enabled() = true; StageProfiler::reset(); }
    ~ProfilerOn() { StageProfiler::enabled() = was; StageProfiler::reset(); }
};

TimeLineCells* tracksOf(TimeLine& timeline)
{
    // The tracks sit right of the layer names.
    const QList<TimeLineCells*> cells = timeline.findChildren<TimeLineCells*>();
    return *std::max_element(cells.cbegin(), cells.cend(), [&](TimeLineCells* a, TimeLineCells* b) {
        return a->mapTo(&timeline, QPoint()).x() < b->mapTo(&timeline, QPoint()).x(); });
}

} // namespace

TEST_CASE("The timeline paints only the keys in view", "[Timeline]")
{
    TestSession session;
    for (int l = 0; l < 3; ++l)
    {
        LayerBitmap* layer = session.object->addNewBitmapLayer();
        for (int f = 2; f <= 2000; ++f) { layer->addNewKeyFrameAt(f); }
    }
    REQUIRE(session.start());
    TimeLine timeline(nullptr);
    timeline.setEditor(session.editor.get());
    timeline.initUI();
    timeline.resize(800, 300);
    timeline.grab(); // lays out the timeline
    TimeLineCells* tracks = tracksOf(timeline);

    for (int offset : { 0, 1000 })
    {
        tracks->hScrollChange(offset);
        ProfilerOn profiler;
        tracks->grab();
        REQUIRE(StageProfiler::calls(StageProfiler::TimelineContent) == 1);
        const int visible = tracks->width() / tracks->getFrameSize();
        const int layers = session.object->getLayerCount();
        CHECK(StageProfiler::calls(StageProfiler::TimelineKey) >= 3u * visible);
        CHECK(StageProfiler::calls(StageProfiler::TimelineKey) <= std::uint64_t(layers) * (visible + 4));
    }
}

TEST_CASE("A Drawing edit repaints the timeline without redrawing its cells", "[Timeline]")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    for (int f = 2; f <= 20; ++f) { layer->addNewKeyFrameAt(f); }
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    TimeLine timeline(nullptr);
    timeline.setEditor(editor);
    timeline.initUI();
    timeline.resize(800, 300);
    timeline.show(); // a hidden widget gets a resize, and so a full redraw, on every grab
    timeline.grab();
    const QImage before = timeline.grab().toImage();

    ProfilerOn profiler;
    timeline.grab();
    CHECK(StageProfiler::calls(StageProfiler::TimelineContent) == 0);
    emit editor->frameModified(5); // pixels only: no cell changes
    CHECK(timeline.grab().toImage() == before);
    CHECK(StageProfiler::calls(StageProfiler::TimelineContent) == 0);

    // A key that comes or goes with the same signal still shows.
    layer->removeKeyFrame(5);
    emit editor->frameModified(5);
    CHECK(timeline.grab().toImage() != before);
    CHECK(StageProfiler::calls(StageProfiler::TimelineContent) == 1);
    layer->addNewKeyFrameAt(5);
    emit editor->frameModified(5);
    CHECK(timeline.grab().toImage() == before);
}

