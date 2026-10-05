#pragma once
#include <Media/Libav.hpp>
#include <Video/VideoEnums.hpp>

#if SCORE_HAS_LIBAV
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <algorithm>

/**
 * @file FrameAccess.hpp
 * @brief Classifies a stream by what reaching an arbitrary frame costs.
 *
 * Scrubbing to a frame means decoding from the keyframe before it, so the
 * longest run of non-keyframes bounds the cost of any seek, on every machine.
 *
 * AV_CODEC_PROP_INTRA_ONLY is not set for CineForm, PNG, FFV1 or all-intra
 * H.264 / HEVC / MPEG-2 (AVC-Intra, XAVC-I, IMX), so the stream itself is
 * read: a container index covering every frame (MP4, MOV) when there is one,
 * else the keyframe flags of the first demuxed packets. Matroska indexes only
 * some keyframes; MPEG-TS and MXF have no index.
 */
namespace Video
{
struct FrameAccessProbe
{
  FrameAccess access{FrameAccess::Unknown};
  int max_gap{-1};
};

namespace keyframe_probe
{
//! XDCAM HD and HDV use 12- to 15-frame GOPs: still worth replaying on a seek.
inline constexpr int max_short_gop = 15;

inline FrameAccess fromGap(int gap) noexcept
{
  return gap == 0                ? FrameAccess::EveryFrame
         : gap <= max_short_gop ? FrameAccess::ShortGop
                                 : FrameAccess::LongGop;
}

inline bool canSeek(const AVFormatContext& fmt) noexcept
{
  if(fmt.pb)
    return fmt.pb->seekable & AVIO_SEEKABLE_NORMAL;
  // Demuxers doing their own I/O (AVFMT_NOFILE): image sequences seek and
  // know their length, live protocols (RTSP...) do not.
  return fmt.duration != AV_NOPTS_VALUE && fmt.duration > 0;
}

//! Longest non-keyframe run from an index that lists every frame.
inline FrameAccessProbe fromIndex(const AVStream& st) noexcept
{
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 78, 100)
  const int n = avformat_index_get_entries_count(&st);
  if(st.nb_frames <= 0 || n < st.nb_frames)
    return {};

  bool sawKey = false;
  int gap = 0, maxGap = 0;
  for(int i = 0; i < n; i++)
  {
    const AVIndexEntry* e = avformat_index_get_entry(const_cast<AVStream*>(&st), i);
    if(e && (e->flags & AVINDEX_KEYFRAME))
    {
      if(sawKey)
        maxGap = std::max(maxGap, gap);
      sawKey = true;
      gap = 0;
    }
    else
    {
      gap++;
    }
  }
  if(!sawKey)
    return {FrameAccess::LongGop, n};
  maxGap = std::max(maxGap, gap);
  return {fromGap(maxGap), maxGap};
#else
  return {};
#endif
}

//! Reads keyframe flags off the first packets of the first video stream,
//! through a context of its own: some demuxers cannot be rewound afterwards
//! (no timestamps to seek on, no byte seeking either).
inline FrameAccessProbe fromPackets(const char* url) noexcept
{
  AVFormatContext* fmt{};
  if(!url || avformat_open_input(&fmt, url, nullptr, nullptr) != 0)
    return {};
  AVPacket* pkt = av_packet_alloc();
  if(!pkt)
  {
    avformat_close_input(&fmt);
    return {};
  }

  // Demuxers that create their streams while reading (MPEG-TS) have none yet:
  // the first video stream a packet belongs to is then the one.
  int video = -1;
  for(unsigned i = 0; i < fmt->nb_streams && video < 0; i++)
    if(fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
      video = int(i);

  // All-intra is settled once more consecutive keyframes than any short GOP
  // went by; a short GOP needs its second keyframe; a long one is settled
  // as soon as a run exceeds the short limit.
  constexpr int max_packets = 2 * (max_short_gop + 1);
  int seen = 0, keys = 0, gap = 0, maxGap = 0;
  bool timed = false;
  while(seen < max_packets && av_read_frame(fmt, pkt) >= 0)
  {
    if(video < 0 && pkt->stream_index < int(fmt->nb_streams)
       && fmt->streams[pkt->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
      video = pkt->stream_index;

    if(pkt->stream_index == video)
    {
      seen++;
      timed |= pkt->pts != AV_NOPTS_VALUE || pkt->dts != AV_NOPTS_VALUE;
      if(pkt->flags & AV_PKT_FLAG_KEY)
      {
        if(keys > 0)
          maxGap = std::max(maxGap, gap);
        keys++;
        gap = 0;
      }
      else
      {
        gap++;
      }
    }
    av_packet_unref(pkt);

    if(gap > max_short_gop)
      break;
    if(keys > max_short_gop && maxGap == 0 && gap == 0)
      break;
  }
  av_packet_free(&pkt);
  avformat_close_input(&fmt);

  if(seen == 0)
    return {};
  // The run still open when reading stopped is at least that long.
  maxGap = std::max(maxGap, gap);
  // Raw elementary streams carry no timestamps: no time can be sought.
  if(!timed)
    return {FrameAccess::Sequential, maxGap};
  if(keys == 0)
    return {FrameAccess::LongGop, maxGap};
  return {fromGap(maxGap), maxGap};
}
}

/**
 * @param url what `fmt` was opened from; the packet probe opens it again so
 *        that `fmt` is not moved.
 */
inline FrameAccessProbe
classifyFrameAccess(AVFormatContext& fmt, AVStream& st, const char* url) noexcept
{
  using namespace keyframe_probe;
  if(!canSeek(fmt))
    return {FrameAccess::Sequential, -1};

  // Raw elementary streams (H.264, HEVC, VVC, MJPEG, Dirac...) carry no
  // timestamps, which their demuxers declare: no time can be sought.
  if(fmt.iformat && (fmt.iformat->flags & AVFMT_NOTIMESTAMPS))
    return {FrameAccess::Sequential, -1};

  if(auto desc = avcodec_descriptor_get(st.codecpar->codec_id);
     desc && (desc->props & AV_CODEC_PROP_INTRA_ONLY))
    return {FrameAccess::EveryFrame, 0};

  if(auto p = fromIndex(st); p.access != FrameAccess::Unknown)
    return p;

  return fromPackets(url);
}
}
#endif
