#pragma once
#include <Media/Libav.hpp>

#if SCORE_HAS_LIBAV
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
}

#include <ossia/detail/flicks.hpp>

#include <algorithm>

/**
 * @file PlaybackTime.hpp
 * @brief Which timestamp a video shows at playback time 0.
 *
 * Every renderer and the thumbnailer go through this, so that they agree on
 * which frame a time shows.
 *
 * Time 0 is the container's start, the earliest timestamp of any stream, as
 * mpv and ffplay rebase it: an MPEG-TS can start hours into its broadcast
 * clock. Using the container's start rather than the video stream's keeps the
 * offset between streams (a video starting after its audio).
 *
 * Never before 0: a negative start is codec delay or edit-list priming (MP4,
 * MOV), whose frames are dropped.
 */
namespace Video
{

//! The timestamp, in `st`'s time base, shown at playback time 0.
inline int64_t playbackStartPts(const AVFormatContext& fmt, const AVStream& st) noexcept
{
  if(fmt.start_time != AV_NOPTS_VALUE && fmt.start_time > 0)
    return av_rescale_q_rnd(
        fmt.start_time, AV_TIME_BASE_Q, st.time_base,
        AVRounding(AV_ROUND_DOWN | AV_ROUND_PASS_MINMAX));
  if(fmt.start_time == AV_NOPTS_VALUE && st.start_time != AV_NOPTS_VALUE)
    return std::max<int64_t>(st.start_time, 0);
  return 0;
}

inline constexpr AVRational flicks_time_base{1, int(ossia::flicks_per_second<int64_t>)};

/**
 * The timestamp a playback time falls on. Rounded down -- in a container
 * whose tick is a whole frame (AVI) rounding to the nearest would switch
 * frames halfway through them -- after a microsecond of slack: a time
 * computed from a frame's start in floating point can land just short of it.
 */
inline int64_t flicksToPts(int64_t flicks, AVRational tb, int64_t start) noexcept
{
  constexpr int64_t slack = ossia::flicks_per_second<int64_t> / 1'000'000;
  return av_rescale_q_rnd(
             flicks + slack, flicks_time_base, tb,
             AVRounding(AV_ROUND_DOWN | AV_ROUND_PASS_MINMAX))
         + start;
}

//! The playback time of a timestamp.
inline int64_t ptsToFlicks(int64_t pts, AVRational tb, int64_t start) noexcept
{
  return av_rescale_q(pts - start, tb, flicks_time_base);
}

}
#endif
