#ifndef STAGEPROFILER_H
#define STAGEPROFILER_H

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <QtGlobal>

// Per-stage time and call counts for the paint path. Off unless JUSTANIMATE_STAGE_PROFILE=1;
// when off, a StageScope costs one branch. Times are inclusive, so InputEvent contains DabMask
// and PaintEvent contains Composite. Main thread only.
class StageProfiler
{
public:
    enum Stage { InputEvent, DabMask, Composite, BufferPaste, PaintEvent, TimelineContent, TimelineKey, StageCount };

    static bool& enabled()
    {
        static bool on = qEnvironmentVariableIntValue("JUSTANIMATE_STAGE_PROFILE") == 1;
        return on;
    }

    static void reset() { for (int i = 0; i < StageCount; ++i) stats()[i] = Stat(); }

    static void report(std::FILE* out = stdout)
    {
        static const char* const names[StageCount] = { "input", "dab mask", "composite", "buffer paste", "canvas update", "timeline", "timeline key" };
        for (int i = 0; i < StageCount; ++i)
            std::fprintf(out, "stage %-13s %8llu calls %10.2f ms\n", names[i],
                         static_cast<unsigned long long>(stats()[i].calls), stats()[i].ns / 1e6);
        std::fflush(out);
    }

    static std::uint64_t calls(Stage s) { return stats()[s].calls; }

private:
    struct Stat { std::uint64_t calls = 0; std::uint64_t ns = 0; };
    static Stat* stats() { static Stat s[StageCount]; return s; }
    friend class StageScope;
};

class StageScope
{
public:
    explicit StageScope(StageProfiler::Stage s) : mOn(StageProfiler::enabled())
    {
        if (mOn)
        {
            mStage = s;
            mStart = std::chrono::steady_clock::now();
        }
    }
    ~StageScope()
    {
        if (mOn)
        {
            StageProfiler::Stat& st = StageProfiler::stats()[mStage];
            st.calls++;
            st.ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - mStart).count());
        }
    }
    StageScope(const StageScope&) = delete;
    StageScope& operator=(const StageScope&) = delete;

private:
    bool mOn;
    StageProfiler::Stage mStage = StageProfiler::InputEvent;
    std::chrono::steady_clock::time_point mStart;
};

#endif // STAGEPROFILER_H
