#include "LibavMediaInfo.hpp"

#if SCORE_HAS_LIBAV

#include <QByteArray>

#include <limits>

namespace score::libav
{
namespace
{

bool has_valid_duration(const AVFormatContext* ctx) noexcept
{
  return ctx && ctx->duration != AV_NOPTS_VALUE
         && ctx->duration != std::numeric_limits<int64_t>::min() && ctx->duration > 0;
}

MediaInfo::FormatContextPtr try_open(const char* path, AVDictionary** opts)
{
  AVFormatContext* raw = avformat_alloc_context();
  if(!raw)
    return {};

  if(avformat_open_input(&raw, path, nullptr, opts) != 0)
    return {};

  MediaInfo::FormatContextPtr ctx{raw};

  if(avformat_find_stream_info(ctx.get(), nullptr) < 0 || ctx->nb_streams == 0)
    return {};

  return ctx;
}

// Walk every packet to compute duration manually. Works on any file whose
// packets are individually readable.
int64_t scan_duration_from_packets(AVFormatContext* ctx) noexcept
{
  // Prefer a video stream; fall back to the first stream of any kind.
  int stream_idx = -1;
  for(unsigned i = 0; i < ctx->nb_streams; i++)
  {
    if(ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      stream_idx = (int)i;
      break;
    }
  }
  if(stream_idx < 0 && ctx->nb_streams > 0)
    stream_idx = 0;
  if(stream_idx < 0)
    return AV_NOPTS_VALUE;

  AVPacket* pkt = av_packet_alloc();
  if(!pkt)
    return AV_NOPTS_VALUE;

  int64_t first_pts = AV_NOPTS_VALUE;
  int64_t last_end_pts = AV_NOPTS_VALUE;
  int64_t packet_count = 0;

  while(av_read_frame(ctx, pkt) >= 0)
  {
    if(pkt->stream_index == stream_idx)
    {
      int64_t ts = (pkt->pts != AV_NOPTS_VALUE) ? pkt->pts : pkt->dts;
      if(ts != AV_NOPTS_VALUE)
      {
        if(first_pts == AV_NOPTS_VALUE)
          first_pts = ts;
        int64_t end = ts + (pkt->duration > 0 ? pkt->duration : 0);
        if(last_end_pts == AV_NOPTS_VALUE || end > last_end_pts)
          last_end_pts = end;
      }
      packet_count++;
    }
    av_packet_unref(pkt);
  }
  av_packet_free(&pkt);

  // Rewind so the context remains usable.
  av_seek_frame(ctx, stream_idx, 0, AVSEEK_FLAG_BACKWARD);

  if(first_pts != AV_NOPTS_VALUE && last_end_pts != AV_NOPTS_VALUE
     && last_end_pts > first_pts)
  {
    AVRational tb = ctx->streams[stream_idx]->time_base;
    return av_rescale_q(last_end_pts - first_pts, tb, AV_TIME_BASE_Q);
  }

  // Last-ditch: estimate from packet count × avg frame rate.
  AVStream* st = ctx->streams[stream_idx];
  if(packet_count > 0 && st->avg_frame_rate.num > 0 && st->avg_frame_rate.den > 0)
  {
    return av_rescale(
        packet_count * (int64_t)AV_TIME_BASE, st->avg_frame_rate.den,
        st->avg_frame_rate.num);
  }

  return AV_NOPTS_VALUE;
}

MediaInfo build_info(MediaInfo::FormatContextPtr ctx, std::optional<int64_t> duration_av)
{
  MediaInfo info;
  info.duration_av = duration_av;
  info.streams.reserve(ctx->nb_streams);
  for(unsigned i = 0; i < ctx->nb_streams; i++)
    info.streams.push_back(ctx->streams[i]->codecpar->codec_type);
  info.format_context = std::move(ctx);
  return info;
}

}

std::optional<MediaInfo> probe(const QString& path)
{
  const QByteArray utf8 = path.toUtf8();
  const char* cpath = utf8.constData();

  // ---- Attempt 1: defaults (fast path) ----
  if(auto ctx = try_open(cpath, nullptr))
  {
    if(has_valid_duration(ctx.get()))
    {
      const int64_t dur = ctx->duration;
      return build_info(std::move(ctx), dur);
    }
    // else: fall through, but drop this context first
  }

  // ---- Attempt 2: large probe + analyze ----
  {
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "probesize", "100000000", 0);
    av_dict_set(&opts, "analyzeduration", "100000000", 0);

    auto ctx = try_open(cpath, &opts);
    av_dict_free(&opts);

    if(ctx && has_valid_duration(ctx.get()))
    {
      const int64_t dur = ctx->duration;
      return build_info(std::move(ctx), dur);
    }
  }

  // ---- Attempt 3: ignore broken index + genpts ----
  {
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "probesize", "100000000", 0);
    av_dict_set(&opts, "analyzeduration", "100000000", 0);
    av_dict_set(&opts, "fflags", "+ignidx+genpts", 0);

    auto ctx = try_open(cpath, &opts);
    av_dict_free(&opts);

    if(ctx)
    {
      if(has_valid_duration(ctx.get()))
      {
        const int64_t dur = ctx->duration;
        return build_info(std::move(ctx), dur);
      }

      // ---- Attempt 4: full packet scan on the same context ----
      int64_t scanned = scan_duration_from_packets(ctx.get());
      if(scanned != AV_NOPTS_VALUE && scanned > 0)
        return build_info(std::move(ctx), scanned);

      // File is structurally readable but no duration could be determined.
      return build_info(std::move(ctx), std::nullopt);
    }
  }

  return std::nullopt;
}

MediaInfo probe_or_throw(const QString& path)
{
  auto info = probe(path);
  if(!info)
    throw std::runtime_error("Couldn't open media file: " + path.toStdString());
  return std::move(*info);
}

} // namespace score::libav

#endif