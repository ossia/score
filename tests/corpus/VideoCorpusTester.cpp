// Corpus tester for score's video decoding: runs one media file through the
// exact decode paths the application uses and reports a machine-readable
// verdict on stdout. Meant to be driven over a whole corpus (the FFmpeg FATE
// suite, generated encode matrices, user libraries) by run-corpus.sh, one
// process per file so a crash or hang condemns only that file.
//
// Modes:
//   video_corpus_tester <file>                 direct decode + reference compare
//   video_corpus_tester --playback <file>      threaded VideoDecoder playback
//   video_corpus_tester --seek-stress <file>   playback with seeks sprinkled in
//
// Direct mode is the correctness oracle: the file is decoded twice in this
// process —
//  - through score's own machinery (VideoDecoder::open + read_one_frame, the
//    same code buffer_thread runs), collecting every produced frame in order;
//  - through an independent, straightforward libavcodec loop applying the
//    same documented policies (first video stream, frames with pts < 0 are
//    dropped, formats without a GPU decoder are converted to RGBA with
//    SWS_FAST_BILINEAR at the stream's declared size).
// Frame-by-frame, pts must match and pixel bytes must hash identically
// (adler32 over the meaningful bytes of every plane, so stride padding and
// buffer reuse don't matter). Every divergence is a decode-loop bug, a
// rescale bug — or a deliberate policy gap, which the report makes visible
// instead of hiding (note "policy dropped N frames").
//
// Exit codes: 0 verdict printed (status says PASS/FAIL kind), 2 usage error.
// Crashes and hangs are the driver's to detect (signal exit / timeout).

#include <Media/Libav.hpp>

#if SCORE_HAS_LIBAV

#include <Video/GpuFormats.hpp>
#include <Video/PlaybackTime.hpp>
#include <Video/VideoDecoder.hpp>

#if SCORE_CORPUS_HAS_GFX
// Everything DirectVideoNodeRenderer.hpp includes comes first, so that only
// its own declarations see private turned public.
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Video/VideoInterface.hpp>

#include <vector>
extern "C" {
#include <libavutil/pixfmt.h>
}
#define private public
#include <Gfx/Graph/DirectVideoNodeRenderer.hpp>
#undef private
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/adler32.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace
{
using clk = std::chrono::steady_clock;

constexpr int max_frames = 2000;
constexpr auto direct_budget = std::chrono::seconds(100);
constexpr auto playback_budget = std::chrono::seconds(120);

struct FrameSig
{
  int64_t pts;
  uint32_t hash;
};

// Libav diagnostics observed while decoding: damaged input makes frame
// content legitimately consumer-dependent (error concealment), and the
// verdict should say so instead of crying wolf.
std::atomic<int> g_av_diagnostics{0};
void counting_log_cb(void*, int level, const char*, va_list)
{
  if(level <= AV_LOG_WARNING)
    g_av_diagnostics.fetch_add(1, std::memory_order_relaxed);
}

// When >= 0, both decode passes append the byte content of the frame at this
// output index here, so a mismatch can be quantified.
struct Capture
{
  int64_t index = -1;
  std::vector<uint8_t> ref, score;
};
Capture g_capture;

void frame_bytes(const AVFrame* f, std::vector<uint8_t>& out)
{
  const auto* desc = av_pix_fmt_desc_get((AVPixelFormat)f->format);
  if(!desc)
  {
    if(f->data[0] && f->linesize[0] > 0)
      out.insert(out.end(), f->data[0], f->data[0] + f->linesize[0]);
    return;
  }
  for(int plane = 0; plane < AV_NUM_DATA_POINTERS; plane++)
  {
    if(!f->data[plane])
      continue;
    if((desc->flags & AV_PIX_FMT_FLAG_PAL) && plane == 1)
    {
      out.insert(out.end(), f->data[1], f->data[1] + 1024);
      continue;
    }
    const int row_bytes
        = av_image_get_linesize((AVPixelFormat)f->format, f->width, plane);
    if(row_bytes <= 0 || f->linesize[plane] == 0)
      continue;
    const int chroma = (plane == 1 || plane == 2);
    const int rows
        = chroma ? AV_CEIL_RSHIFT(f->height, desc->log2_chroma_h) : f->height;
    for(int y = 0; y < rows; y++)
    {
      const uint8_t* row = f->data[plane] + int64_t(y) * f->linesize[plane];
      out.insert(out.end(), row, row + row_bytes);
    }
  }
}

struct Verdict
{
  std::string status;
  std::string note;
  int64_t score_frames = -1;
  int64_t ref_frames = -1;
  int64_t ref_raw_frames = -1;
  int64_t first_mismatch = -1;
  std::string native_format;
  // hwdec mode only
  std::string requested; // accel asked for on the command line
  std::string engaged;   // what actually decoded: device type / codec / "sw"
  std::string out_format; // pixel format score's frames arrived in
  // Further fields, already JSON ("\"key\":value,..."), emitted as they are.
  std::string extra;
};

std::string json_escape(const std::string& s)
{
  std::string r;
  r.reserve(s.size());
  for(char c : s)
  {
    if(c == '"' || c == '\\')
      r += '\\';
    if((unsigned char)c < 0x20)
    {
      r += ' ';
      continue;
    }
    r += c;
  }
  return r;
}

void emit(const char* mode, const std::string& file, const Verdict& v)
{
  std::string hw;
  if(!v.requested.empty())
    hw = ",\"requested\":\"" + json_escape(v.requested) + "\",\"engaged\":\""
         + json_escape(v.engaged) + "\",\"out_format\":\"" + json_escape(v.out_format)
         + "\"";
  if(!v.extra.empty())
    hw += "," + v.extra;
  std::printf(
      "{\"mode\":\"%s\",\"file\":\"%s\",\"status\":\"%s\",\"score_frames\":%" PRId64
      ",\"ref_frames\":%" PRId64 ",\"ref_raw_frames\":%" PRId64
      ",\"first_mismatch\":%" PRId64 ",\"native_format\":\"%s\"%s,\"note\":\"%s\"}\n",
      mode, json_escape(file).c_str(), v.status.c_str(), v.score_frames, v.ref_frames,
      v.ref_raw_frames, v.first_mismatch, json_escape(v.native_format).c_str(),
      hw.c_str(), json_escape(v.note).c_str());
  std::fflush(stdout);
}

void note_append(std::string& note, const std::string& what)
{
  if(!note.empty())
    note += "; ";
  note += what;
}

// Hash the meaningful pixel bytes of a frame: per plane, per row, the bytes a
// row of this width actually occupies — never the stride padding, so frames
// from differently-allocated buffers compare equal iff their content is.
uint32_t hash_frame_pixels(const AVFrame* f)
{
  uint32_t h = 1;
  const auto* desc = av_pix_fmt_desc_get((AVPixelFormat)f->format);
  if(!desc)
  {
    // Not a real AVPixelFormat (score's GPU-direct paths store a fourcc):
    // hash the raw payload.
    if(f->data[0] && f->linesize[0] > 0)
      h = av_adler32_update(h, f->data[0], f->linesize[0]);
    return h;
  }

  for(int plane = 0; plane < AV_NUM_DATA_POINTERS; plane++)
  {
    if(!f->data[plane])
      continue;

    if((desc->flags & AV_PIX_FMT_FLAG_PAL) && plane == 1)
    {
      h = av_adler32_update(h, f->data[1], 1024);
      continue;
    }

    const int row_bytes
        = av_image_get_linesize((AVPixelFormat)f->format, f->width, plane);
    if(row_bytes <= 0)
      continue;

    const int chroma = (plane == 1 || plane == 2);
    const int rows
        = chroma ? AV_CEIL_RSHIFT(f->height, desc->log2_chroma_h) : f->height;
    const int stride = f->linesize[plane];
    if(stride == 0)
      continue;

    for(int y = 0; y < rows; y++)
    {
      // data[plane] always points at the visually-first row; a bottom-up
      // frame just has a negative stride. Same formula for both.
      const uint8_t* row = f->data[plane] + int64_t(y) * stride;
      h = av_adler32_update(h, row, row_bytes);
    }
  }
  return h;
}

// Debug aid: VIDEO_TESTER_DUMPFRAME=<n> writes frame n from both paths as raw
// planes (same byte walk as the hash) to /tmp/<side>_f<n>.raw for cmp/ffmpeg.
void maybe_dump_frame(const char* side, size_t index, const AVFrame* f)
{
  const char* want = getenv("VIDEO_TESTER_DUMPFRAME");
  if(!want || size_t(atoll(want)) != index)
    return;
  char path[256];
  snprintf(path, sizeof path, "/tmp/%s_f%zu.raw", side, index);
  FILE* out = fopen(path, "wb");
  if(!out)
    return;
  const auto* desc = av_pix_fmt_desc_get((AVPixelFormat)f->format);
  if(desc)
  {
    for(int plane = 0; plane < AV_NUM_DATA_POINTERS; plane++)
    {
      if(!f->data[plane])
        continue;
      const int row_bytes
          = av_image_get_linesize((AVPixelFormat)f->format, f->width, plane);
      if(row_bytes <= 0)
        continue;
      const int chroma = (plane == 1 || plane == 2);
      const int rows
          = chroma ? AV_CEIL_RSHIFT(f->height, desc->log2_chroma_h) : f->height;
      for(int y = 0; y < rows; y++)
        fwrite(f->data[plane] + int64_t(y) * f->linesize[plane], 1, row_bytes, out);
    }
  }
  fclose(out);
}

// ---------------------------------------------------------------------------
// Reference decode: an intentionally boring, by-the-book libavcodec loop.
// ---------------------------------------------------------------------------

struct Reference
{
  bool opened = false;
  bool raw_gpu = false; // HAP/DXV: score forwards packets undecoded
  std::vector<FrameSig> frames;
  int64_t raw_frames = 0; // before the pts >= 0 policy
  std::string native_format;
  std::string note;
  AVPixelFormat out_fmt = AV_PIX_FMT_NONE; // format the compared frames are in
};

// score forwards these codecs' packets straight to the GPU; the reference for
// them is the demuxed packet payload, not decoded pixels.
bool is_raw_gpu_codec(AVCodecID id)
{
  return id == AV_CODEC_ID_HAP || id == AV_CODEC_ID_DXV;
}

// raw_mode: 1 = compare against demuxed packets (score chose the GPU-direct
// path), 0 = compare against decoded pixels, -1 = guess from the codec id
// (score's own decoder could not be opened, counts are informational only).
// ignore_pts mirrors DecoderConfiguration::ignorePTS on the reference side:
// hw validation uses it so that raw elementary streams (conformance suites,
// whose frames all have no pts) still compare decoder against decoder.
// rescale = false compares native frames whatever their format, as the
// direct renderer hands every decoded frame to a GPU decoder.
Reference reference_decode(
    const std::string& path, clk::time_point deadline, int raw_mode,
    bool ignore_pts = false, bool rescale = true)
{
  Reference ref;

  AVFormatContext* fmt{};
  if(avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) != 0)
    return ref;
  if(avformat_find_stream_info(fmt, nullptr) < 0)
  {
    avformat_close_input(&fmt);
    return ref;
  }

  // Same selection rule as VideoDecoder::open_stream: first video stream.
  int stream = -1;
  for(unsigned i = 0; i < fmt->nb_streams; i++)
  {
    if(fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      stream = int(i);
      break;
    }
  }
  if(stream < 0)
  {
    avformat_close_input(&fmt);
    return ref;
  }
  for(unsigned i = 0; i < fmt->nb_streams; i++)
    if(int(i) != stream)
      fmt->streams[i]->discard = AVDISCARD_ALL;

  AVStream* st = fmt->streams[stream];
  const auto par = st->codecpar;

  if(const auto* d = av_pix_fmt_desc_get((AVPixelFormat)par->format))
    ref.native_format = d->name;
  else
    ref.native_format = "unknown(" + std::to_string(par->format) + ")";

  const bool as_raw = raw_mode == 1 || (raw_mode == -1 && is_raw_gpu_codec(par->codec_id));
  if(as_raw)
  {
    // Reference = the demuxed packets themselves.
    ref.raw_gpu = true;
    ref.opened = true;
    AVPacket* pkt = av_packet_alloc();
    while(av_read_frame(fmt, pkt) >= 0 && int64_t(ref.frames.size()) < max_frames
          && clk::now() < deadline)
    {
      if(pkt->stream_index == stream && pkt->size > 0)
      {
        uint32_t h = av_adler32_update(1, pkt->data, pkt->size);
        ref.frames.push_back({pkt->pts, h});
        ref.raw_frames++;
      }
      av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    avformat_close_input(&fmt);
    return ref;
  }

  const AVCodec* codec = avcodec_find_decoder(par->codec_id);
  if(!codec)
  {
    avformat_close_input(&fmt);
    return ref;
  }
  AVCodecContext* ctx = avcodec_alloc_context3(codec);
  if(avcodec_parameters_to_context(ctx, par) < 0 || par->width <= 0
     || par->height <= 0)
  {
    // Mirror VideoDecoder::open_stream's refusal of size-less streams so the
    // comparison is against what score is *supposed* to handle.
    avcodec_free_context(&ctx);
    avformat_close_input(&fmt);
    return ref;
  }
  ctx->pkt_timebase = st->time_base;
  ctx->framerate = av_guess_frame_rate(fmt, st, nullptr); // as init_codec_context's setup does
  ctx->thread_count = 0; // same as DecoderConfiguration{}.threads
  if(avcodec_open2(ctx, codec, nullptr) < 0)
  {
    avcodec_free_context(&ctx);
    avformat_close_input(&fmt);
    return ref;
  }
  ref.opened = true;

  // score converts formats without a GPU decoder to RGBA at the stream's
  // declared size, with SWS_FAST_BILINEAR (Video/Rescale.cpp).
  const bool needs_rescale
      = rescale && Video::formatNeedsDecoding((AVPixelFormat)par->format);
  SwsContext* sws{};
  AVPixelFormat sws_src_fmt = AV_PIX_FMT_NONE;
  AVFrame* rgb = nullptr;

  AVPacket* pkt = av_packet_alloc();
  AVFrame* f = av_frame_alloc();

  // receiveVideoFrame's numbering of frames without a timestamp: the
  // best-effort one, else the previous frame's plus one frame at the rate.
  int64_t missing_next = 0, missing_step = 1;
  {
    const AVRational rate
        = st->avg_frame_rate.num > 0 ? st->avg_frame_rate : st->r_frame_rate;
    if(rate.num > 0 && rate.den > 0)
      missing_step = std::max<int64_t>(1, av_rescale_q(1, av_inv_q(rate), st->time_base));
  }

  auto take_frame = [&](AVFrame* frame) {
    ref.raw_frames++;
    if(frame->pts == AV_NOPTS_VALUE)
      frame->pts = frame->best_effort_timestamp;
    int64_t duration = missing_step;
    if(frame->pts == AV_NOPTS_VALUE)
      frame->pts = missing_next;
    else
#if(LIBAVUTIL_VERSION_MAJOR < 58)
      duration = frame->pkt_duration > 0 ? frame->pkt_duration : missing_step;
#else
      duration = frame->duration > 0 ? frame->duration : missing_step;
#endif
    missing_next = frame->pts + duration;
    if(!ignore_pts && frame->pts < 0) // receiveVideoFrame's policy
      return;
    if(int64_t(ref.frames.size()) >= max_frames)
      return;

    if(needs_rescale)
    {
      if(!sws || sws_src_fmt != (AVPixelFormat)frame->format)
      {
        sws_freeContext(sws);
        sws_src_fmt = (AVPixelFormat)frame->format;
        sws = sws_getContext(
            par->width, par->height, sws_src_fmt, par->width, par->height,
            AV_PIX_FMT_RGBA, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
      }
      if(!sws)
      {
        note_append(ref.note, "ref sws_getContext failed");
        return;
      }
      if(!rgb)
      {
        rgb = av_frame_alloc();
        rgb->width = par->width;
        rgb->height = par->height;
        rgb->format = AV_PIX_FMT_RGBA;
        av_frame_get_buffer(rgb, 0);
      }
      if(frame->width != par->width || frame->height != par->height)
        note_append(ref.note, "frame dims differ from stream dims");
      sws_scale(
          sws, frame->data, frame->linesize, 0, par->height, rgb->data,
          rgb->linesize);
      if(g_capture.index == int64_t(ref.frames.size()))
        frame_bytes(rgb, g_capture.ref);
      ref.out_fmt = AV_PIX_FMT_RGBA;
      ref.frames.push_back({frame->pts, hash_frame_pixels(rgb)});
    }
    else
    {
      maybe_dump_frame("ref", ref.frames.size(), frame);
      if(g_capture.index == int64_t(ref.frames.size()))
        frame_bytes(frame, g_capture.ref);
      ref.out_fmt = (AVPixelFormat)frame->format;
      ref.frames.push_back({frame->pts, hash_frame_pixels(frame)});
    }
  };

  auto drain = [&] {
    for(;;)
    {
      int r = avcodec_receive_frame(ctx, f);
      if(r < 0)
        return r;
      take_frame(f);
      av_frame_unref(f);
    }
  };

  int read_err = 0;
  while((read_err = av_read_frame(fmt, pkt)) >= 0
        && int64_t(ref.frames.size()) < max_frames && clk::now() < deadline)
  {
    if(pkt->stream_index == stream)
    {
      int s = avcodec_send_packet(ctx, pkt);
      if(s == AVERROR(EAGAIN))
      {
        drain();
        s = avcodec_send_packet(ctx, pkt);
      }
      (void)s; // corrupt packets are allowed to fail; keep going like ffmpeg does
      drain();
    }
    av_packet_unref(pkt);
  }
  avcodec_send_packet(ctx, nullptr);
  drain();

  av_frame_free(&rgb);
  sws_freeContext(sws);
  av_frame_free(&f);
  av_packet_free(&pkt);
  avcodec_free_context(&ctx);
  avformat_close_input(&fmt);
  return ref;
}

// ---------------------------------------------------------------------------
// score decode, direct: the code buffer_thread runs, minus thread and sleeps.
// ---------------------------------------------------------------------------

struct ScoreResult
{
  bool opened = false;
  bool raw_gpu = false; // open_stream chose the packet-forwarding path
  std::vector<FrameSig> frames;
  std::string note;
  // hwdec introspection
  std::string engaged_device; // hw_device_ctx's type name, if any
  std::string codec_name;     // the decoder that was actually opened
  std::string out_format;     // format of the first produced frame
};

ScoreResult score_decode_direct(
    const std::string& path, clk::time_point deadline,
    const Video::DecoderConfiguration& conf = Video::DecoderConfiguration{},
    AVPixelFormat normalize_to = AV_PIX_FMT_NONE)
{
  ScoreResult res;

  Video::VideoDecoder dec{conf};
  if(!dec.open(path))
    return res;
  res.opened = true;
  res.raw_gpu = !dec.m_conf.useAVCodec;

  if(dec.m_codecContext)
  {
    if(dec.m_codecContext->codec && dec.m_codecContext->codec->name)
      res.codec_name = dec.m_codecContext->codec->name;
    if(dec.m_codecContext->hw_device_ctx)
    {
      auto* hw = (AVHWDeviceContext*)dec.m_codecContext->hw_device_ctx->data;
      if(const char* n = av_hwdevice_get_type_name(hw->type))
        res.engaged_device = n;
    }
  }

  // hwdec output usually arrives as NV12/P010 after the hw->sw transfer while
  // the software reference decodes to the codec's native format: compare in
  // the reference's format, through an exact repack conversion.
  SwsContext* norm_sws{};
  AVPixelFormat norm_src = AV_PIX_FMT_NONE;
  AVFrame* norm_frame{};
  auto normalize = [&](AVFrame* fr) -> AVFrame* {
    if(normalize_to == AV_PIX_FMT_NONE || fr->format == normalize_to
       || !av_pix_fmt_desc_get((AVPixelFormat)fr->format))
      return fr;
    if(!norm_sws || norm_src != (AVPixelFormat)fr->format)
    {
      sws_freeContext(norm_sws);
      norm_src = (AVPixelFormat)fr->format;
      norm_sws = sws_getContext(
          fr->width, fr->height, norm_src, fr->width, fr->height, normalize_to,
          SWS_POINT, nullptr, nullptr, nullptr);
      if(!norm_sws)
      {
        note_append(
            res.note, std::string("cannot normalize ")
                          + av_get_pix_fmt_name(norm_src) + " to "
                          + av_get_pix_fmt_name(normalize_to));
        return fr;
      }
      av_frame_free(&norm_frame);
    }
    if(!norm_frame)
    {
      norm_frame = av_frame_alloc();
      norm_frame->width = fr->width;
      norm_frame->height = fr->height;
      norm_frame->format = normalize_to;
      av_frame_get_buffer(norm_frame, 0);
    }
    sws_scale(
        norm_sws, fr->data, fr->linesize, 0, fr->height, norm_frame->data,
        norm_frame->linesize);
    norm_frame->pts = fr->pts;
    return norm_frame;
  };

  auto collect_one = [&](AVFrame* fr) {
    if(int64_t(res.frames.size()) < max_frames)
    {
      if(res.out_format.empty())
      {
        if(const char* n = av_get_pix_fmt_name((AVPixelFormat)fr->format))
          res.out_format = n;
        else
          res.out_format = "fourcc";
      }
      AVFrame* view = normalize(fr);
      maybe_dump_frame("score", res.frames.size(), view);
      if(g_capture.index == int64_t(res.frames.size()))
        frame_bytes(view, g_capture.score);
      res.frames.push_back({fr->pts, hash_frame_pixels(view)});
    }
    dec.m_frames.release(fr);
  };

  AVPacket* pkt = av_packet_alloc();
  int stuck = 0;
  while(!dec.m_finished && int64_t(res.frames.size()) < max_frames
        && clk::now() < deadline)
  {
    av_packet_unref(pkt);
    auto r = dec.read_one_frame(*pkt);

    // A call can have enqueued frames beyond the one it returns; queue order
    // comes first, the returned frame is the newest.
    bool got = false;
    while(AVFrame* fr = dec.m_frames.dequeue_one())
    {
      collect_one(fr);
      got = true;
    }
    if(r.frame)
    {
      collect_one(r.frame);
      got = true;
    }

    if(got)
      stuck = 0;
    else if(++stuck > 1000)
    {
      note_append(res.note, "no progress for 1000 reads, err " + std::to_string(r.error));
      break;
    }
  }
  // EOF flush may have filled the queue on the very last call.
  while(AVFrame* fr = dec.m_frames.dequeue_one())
    collect_one(fr);

  av_packet_unref(pkt);
  av_packet_free(&pkt);
  av_frame_free(&norm_frame);
  sws_freeContext(norm_sws);

  if(int64_t(res.frames.size()) >= max_frames)
    note_append(res.note, "frame cap reached");
  if(clk::now() >= deadline)
    note_append(res.note, "time budget reached");
  return res;
}

int run_direct(const std::string& path)
{
  const auto t0 = clk::now();
  Verdict v;

  // score first: its open decides which representation ("decoded pixels" or
  // "forwarded packets") the reference has to produce for a fair comparison.
  auto sc = score_decode_direct(path, t0 + direct_budget);
  v.score_frames = sc.opened ? int64_t(sc.frames.size()) : -1;

  auto ref = reference_decode(
      path, t0 + direct_budget, sc.opened ? int(sc.raw_gpu) : -1);
  v.ref_frames = ref.opened ? int64_t(ref.frames.size()) : -1;
  v.ref_raw_frames = ref.opened ? ref.raw_frames : -1;
  v.native_format = ref.native_format;
  v.note = ref.note;
  if(!sc.note.empty())
    note_append(v.note, sc.note);

  if(getenv("VIDEO_TESTER_DUMP"))
  {
    const size_t n = std::max(ref.frames.size(), sc.frames.size());
    for(size_t i = 0; i < n; i++)
    {
      auto fmt = [](const std::vector<FrameSig>& fs, size_t i) -> std::string {
        if(i >= fs.size())
          return "-";
        return std::to_string(fs[i].pts) + "/" + std::to_string(fs[i].hash);
      };
      std::fprintf(
          stderr, "%4zu ref %-24s score %-24s%s\n", i, fmt(ref.frames, i).c_str(),
          fmt(sc.frames, i).c_str(),
          (i < ref.frames.size() && i < sc.frames.size()
           && (ref.frames[i].pts != sc.frames[i].pts
               || ref.frames[i].hash != sc.frames[i].hash))
              ? "   <-- differs"
              : "");
    }
  }

  if(!ref.opened && !sc.opened)
    v.status = "SKIP"; // not a video either way
  else if(!sc.opened)
    v.status = ref.frames.empty() ? "SKIP" : "SCORE_CANT_OPEN";
  else if(!ref.opened)
  {
    v.status = "REF_CANT_OPEN"; // score opens more than plain libav? note it
  }
  else
  {
    // Compare the common prefix; then lengths.
    const size_t n = std::min(ref.frames.size(), sc.frames.size());
    size_t bad = n;
    for(size_t i = 0; i < n; i++)
    {
      if(ref.frames[i].pts != sc.frames[i].pts
         || ref.frames[i].hash != sc.frames[i].hash)
      {
        bad = i;
        break;
      }
    }
    if(bad < n)
    {
      v.first_mismatch = int64_t(bad);
      if(ref.frames[bad].pts != sc.frames[bad].pts)
        v.status = "PTS_MISMATCH";
      else
      {
        // Quantify the divergence: decode both sides again, capturing the
        // bytes of the diverging frame. A handful of differing bytes is
        // error-concealment noise on damaged or consumer-sensitive files
        // (the reference loop and ffmpeg's own CLI diverge on those too);
        // wide divergence is a real decode bug.
        g_capture = {};
        g_capture.index = int64_t(bad);
        const auto t1 = clk::now();
        score_decode_direct(path, t1 + direct_budget);
        reference_decode(path, t1 + direct_budget, sc.opened ? int(sc.raw_gpu) : -1);

        size_t diff_bytes = 0;
        const size_t total = std::min(g_capture.ref.size(), g_capture.score.size());
        for(size_t i = 0; i < total; i++)
          diff_bytes += (g_capture.ref[i] != g_capture.score[i]);
        diff_bytes += std::max(g_capture.ref.size(), g_capture.score.size()) - total;

        note_append(
            v.note, "frame " + std::to_string(bad) + ": " + std::to_string(diff_bytes)
                        + "/" + std::to_string(total) + " bytes differ");
        const bool minor
            = total > 0 && diff_bytes <= std::max<size_t>(64, total / 200);
        // On damaged input the bytes past the damage point are pool-history
        // noise, not a defined result: ea-wve/networkBackbone-partial.wve
        // diverges 55% between this reference and ffmpeg's own CLI, while
        // score matches the CLI byte for byte. Flag it for triage instead of
        // calling it a failure.
        const bool damaged = g_av_diagnostics.load(std::memory_order_relaxed) > 0;
        v.status = minor      ? "PIXEL_MISMATCH_MINOR"
                   : damaged ? "PIXEL_MISMATCH_DAMAGED"
                             : "PIXEL_MISMATCH";
        g_capture = {};
      }
    }
    else if(ref.frames.size() != sc.frames.size())
      v.status = "COUNT_MISMATCH";
    else if(ref.frames.empty() && ref.raw_frames > 0)
    {
      v.status = "OK";
      note_append(
          v.note,
          "policy dropped all " + std::to_string(ref.raw_frames) + " frames (pts<0)");
    }
    else
    {
      v.status = "OK";
      if(ref.raw_frames > int64_t(ref.frames.size()))
        note_append(
            v.note, "policy dropped "
                        + std::to_string(ref.raw_frames - int64_t(ref.frames.size()))
                        + " frames (pts<0)");
    }
  }

  if(int n = g_av_diagnostics.load(std::memory_order_relaxed))
    note_append(v.note, "libav diagnostics: " + std::to_string(n));

  emit("direct", path, v);
  return 0;
}

// Whether plain libavcodec gets at least one frame out of the file: a decode
// that produced none is a failure only then.
bool libav_decodes_a_frame(const std::string& path)
{
  AVFormatContext* fmt{};
  if(avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) != 0)
    return false;
  bool got = false;
  if(avformat_find_stream_info(fmt, nullptr) >= 0)
  {
    const int s = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    const AVCodec* codec
        = s >= 0 ? avcodec_find_decoder(fmt->streams[s]->codecpar->codec_id) : nullptr;
    AVCodecContext* ctx = codec ? avcodec_alloc_context3(codec) : nullptr;
    if(ctx && avcodec_parameters_to_context(ctx, fmt->streams[s]->codecpar) >= 0
       && avcodec_open2(ctx, codec, nullptr) == 0)
    {
      AVPacket* pkt = av_packet_alloc();
      AVFrame* f = av_frame_alloc();
      for(int n = 0; !got && n < 256 && av_read_frame(fmt, pkt) >= 0; n++)
      {
        if(pkt->stream_index == s && avcodec_send_packet(ctx, pkt) >= 0)
          got = avcodec_receive_frame(ctx, f) == 0;
        av_packet_unref(pkt);
      }
      if(!got && avcodec_send_packet(ctx, nullptr) >= 0)
        got = avcodec_receive_frame(ctx, f) == 0;
      av_frame_free(&f);
      av_packet_free(&pkt);
    }
    avcodec_free_context(&ctx);
  }
  avformat_close_input(&fmt);
  return got;
}

#if SCORE_CORPUS_HAS_GFX
// ---------------------------------------------------------------------------
// The direct renderer: DirectVideoNodeRenderer's own decoder, which Auto
// playback uses for every source whose frames are all keyframes. Driven the
// way its update() drives it -- a time since the start of playback in, the
// frame showing at that time out -- first in order, then at random times,
// which is scrubbing. Each answer must be the reference's frame for that
// time, by pts and by pixels.
// ---------------------------------------------------------------------------

const char* frame_access_name(Video::FrameAccess a)
{
  switch(a)
  {
    case Video::FrameAccess::EveryFrame:
      return "EveryFrame";
    case Video::FrameAccess::ShortGop:
      return "ShortGop";
    case Video::FrameAccess::LongGop:
      return "LongGop";
    case Video::FrameAccess::Sequential:
      return "Sequential";
    default:
      return "Unknown";
  }
}

struct DirectFrame
{
  bool ok = false;
  int64_t pts = 0;
  uint32_t hash = 0;
};

DirectFrame direct_frame(const score::gfx::DirectVideoNodeRenderer& r)
{
  const AVFrame* f = r.m_decodedFrame;
  if(!f || !f->data[0])
    return {};
  // HAP / DXV: the frame carries the demuxed packet, hashed as the reference
  // hashes packets.
  if(!r.m_useAVCodec)
    return {true, f->pts, uint32_t(av_adler32_update(1, f->data[0], f->linesize[0]))};
  // The timestamp the renderer places the frame at: its best-effort one, or
  // the number it gave a frame without any. A dts-only file (AVI) has none in
  // pts.
  return {true, r.m_lastDecodedDts, hash_frame_pixels(f)};
}

int run_direct_renderer(const std::string& path)
{
  constexpr const char* mode = "direct_renderer";
  constexpr size_t max_sequential = 300;
  const auto t0 = clk::now();
  const auto deadline = t0 + direct_budget;
  Verdict v;

  auto dec = std::make_shared<Video::VideoDecoder>(Video::DecoderConfiguration{});
  if(!dec->open(path))
  {
    auto ref = reference_decode(path, deadline, -1, false, false);
    v.status = ref.opened && !ref.frames.empty() ? "SCORE_CANT_OPEN" : "SKIP";
    emit(mode, path, v);
    return 0;
  }

  // VIDEO_TESTER_FORCE_DIRECT checks them anyway, as a user forcing Direct
  // playback on one would play them.
  if(dec->frame_access == Video::FrameAccess::Sequential
     && !getenv("VIDEO_TESTER_FORCE_DIRECT"))
  {
    v.status = "NOT_APPLICABLE";
    v.extra = "\"frame_access\":\"Sequential\"";
    note_append(v.note, "no time can be sought; Auto plays it from the frame queue");
    emit(mode, path, v);
    return 0;
  }

  score::gfx::VideoNode node{dec, std::nullopt};
  score::gfx::DirectVideoNodeRenderer r{node, *dec};
  if(!r.openFile(score::gfx::GraphicsApi::Null, nullptr))
  {
    v.status = "RENDERER_CANT_OPEN";
    emit(mode, path, v);
    return 0;
  }

  auto ref = reference_decode(path, deadline, int(!r.m_useAVCodec), false, false);
  v.ref_frames = ref.opened ? int64_t(ref.frames.size()) : -1;
  v.ref_raw_frames = ref.opened ? ref.raw_frames : -1;
  v.native_format = ref.native_format;
  v.note = ref.note;

  std::string extra = std::string("\"frame_access\":\"")
                      + frame_access_name(dec->frame_access) + "\",\"max_keyframe_gap\":"
                      + std::to_string(dec->max_keyframe_gap);
  if(const AVCodecContext* c = r.m_codecContext)
  {
    const char* model = c->active_thread_type == FF_THREAD_FRAME   ? "frame"
                        : c->active_thread_type == FF_THREAD_SLICE ? "slice"
                                                                   : "none";
    extra += std::string(",\"threading\":\"") + model + "\",\"thread_count\":"
             + std::to_string(c->thread_count);
  }
  else
  {
    extra += ",\"threading\":\"gpu-direct\"";
  }

  auto finish = [&] {
    v.extra = extra;
    if(int n = g_av_diagnostics.load(std::memory_order_relaxed))
      note_append(v.note, "libav diagnostics: " + std::to_string(n));
    emit(mode, path, v);
    return 0;
  };

  if(!ref.opened || ref.frames.empty())
  {
    v.status = "SKIP";
    return finish();
  }
  if(ref.frames.front().pts == AV_NOPTS_VALUE)
  {
    v.status = "NOT_APPLICABLE";
    note_append(v.note, "no timestamps: the renderer cannot map a time to a frame");
    return finish();
  }

  const auto& frames = ref.frames;
  // Playback time 0 is the stream's start, or 0 when it starts before: frames
  // before 0 are dropped. Frames that fail to decode at the start (damaged
  // files) leave the first decoded one later than that.
  // Playback time 0: the container's start (playbackStartPts), which the
  // frame queue maps time from. The direct renderer must take the same.
  const int64_t first_pts = dec->start_pts;
  if(r.m_startPts != dec->start_pts)
  {
    note_append(
        v.note, "time 0 is pts " + std::to_string(r.m_startPts)
                    + " for the direct renderer, " + std::to_string(dec->start_pts)
                    + " for the frame queue");
    v.status = "START_MISMATCH";
    return finish();
  }
  const double flicks_per_dts = r.m_flicks_per_dts;
  auto request = [&](size_t i, bool absolute) {
    const int64_t pts = frames[i].pts - (absolute ? 0 : first_pts);
    return int64_t(double(pts) * flicks_per_dts);
  };
  // Halfway into frame i: what playback asks between two frame starts.
  auto request_mid = [&](size_t i) {
    const int64_t next = i + 1 < frames.size() && frames[i + 1].pts > frames[i].pts
                             ? frames[i + 1].pts
                             : frames[i].pts + 1;
    const int64_t pts = (frames[i].pts + next) / 2 - first_pts;
    return int64_t(double(pts) * flicks_per_dts);
  };

  struct Timing
  {
    double total_ms = 0., max_ms = 0.;
    int count = 0;
    void add(clk::duration d)
    {
      const double ms = std::chrono::duration<double, std::milli>(d).count();
      total_ms += ms;
      max_ms = std::max(max_ms, ms);
      count++;
    }
    std::string json(const char* key) const
    {
      char buf[128];
      std::snprintf(
          buf, sizeof(buf), ",\"%s_ms_avg\":%.2f,\"%s_ms_max\":%.2f,\"%s_checked\":%d",
          key, count ? total_ms / count : 0., key, max_ms, key, count);
      return buf;
    }
  };

  // The reference frame a decoded one is, if any: identifies a wrong frame.
  auto find_ref = [&](const DirectFrame& got) -> int64_t {
    for(size_t j = 0; j < frames.size(); j++)
      if(frames[j].pts == got.pts && frames[j].hash == got.hash)
        return int64_t(j);
    return -1;
  };

  // update() keeps the frame it has while it holds the time, and decodes
  // otherwise.
  auto shows = [&](int64_t flicks) {
    if(direct_frame(r).ok && r.holdsTime(flicks))
      return true;
    return r.seekAndDecode(flicks);
  };

  // Returns the failing status, or nullptr when frame i came out.
  auto check = [&](size_t i, bool absolute, Timing& timing) -> const char* {
    const auto t = clk::now();
    const bool decoded = shows(request(i, absolute));
    timing.add(clk::now() - t);
    const auto got = direct_frame(r);
    if(!decoded || !got.ok)
      return "NO_FRAME";
    if(got.pts == frames[i].pts && got.hash == frames[i].hash)
      return nullptr;
    if(const auto j = find_ref(got); j >= 0)
    {
      note_append(
          v.note, "frame " + std::to_string(i) + " asked, frame " + std::to_string(j)
                      + " shown");
      // Asked again from a fresh seek, halfway into the frame: when that
      // gives it, only a time exactly on the frame's start is missed.
      r.m_lastDecodedDts = INT64_MIN;
      const bool again = r.seekAndDecode(request_mid(i));
      const auto mid = direct_frame(r);
      r.m_lastDecodedDts = INT64_MIN;
      if(again && mid.ok && mid.pts == frames[i].pts && mid.hash == frames[i].hash)
      {
        note_append(v.note, "shown when asked halfway into the frame");
        return "BOUNDARY";
      }
      return "WRONG_FRAME";
    }
    if(got.pts == frames[i].pts)
      return g_av_diagnostics.load(std::memory_order_relaxed) > 0
                 ? "PIXEL_MISMATCH_DAMAGED"
                 : "PIXEL_MISMATCH";
    note_append(
        v.note, "frame " + std::to_string(i) + " asked, pts " + std::to_string(got.pts)
                    + " shown, which the reference does not have");
    return "PTS_MISMATCH";
  };

  // In order, as playback asks.
  Timing sequential;
  const size_t n = std::min(frames.size(), max_sequential);
  const char* failure = nullptr;
  for(size_t i = 0; i < n && clk::now() < deadline; i++)
  {
    if((failure = check(i, false, sequential)))
    {
      v.first_mismatch = int64_t(i);
      break;
    }
  }
  v.score_frames = sequential.count;

  // At random, as scrubbing asks. Long GOPs replay from their keyframe on
  // every seek: fewer of them keep the run bounded.
  Timing scrub;
  if(!failure)
  {
    const bool cheap = !r.m_useAVCodec
                       || dec->frame_access == Video::FrameAccess::EveryFrame
                       || dec->frame_access == Video::FrameAccess::ShortGop;
    const int seeks = std::min<int>(int(frames.size()), cheap ? 40 : 10);
    std::mt19937 rng{1234};
    std::uniform_int_distribution<size_t> pick{0, frames.size() - 1};
    for(int k = 0; k < seeks && clk::now() < deadline; k++)
    {
      const size_t i = pick(rng);
      if(const char* f = check(i, false, scrub))
      {
        failure = f;
        v.status = std::string("SCRUB_") + f;
        v.first_mismatch = int64_t(i);
        break;
      }
    }
  }
  // Backwards, as score plays when its speed is negative: halfway into each
  // frame, last first, as update() asks.
  Timing reverse;
  if(!failure)
  {
    const size_t last = std::min(frames.size(), max_sequential) - 1;
    for(size_t k = 0; k < 30 && k <= last && clk::now() < deadline; k++)
    {
      const size_t i = last - k;
      const auto t = clk::now();
      const bool decoded = shows(request_mid(i));
      reverse.add(clk::now() - t);
      const auto got = direct_frame(r);
      if(!decoded || !got.ok || got.pts != frames[i].pts || got.hash != frames[i].hash)
      {
        const auto j = got.ok ? find_ref(got) : -1;
        note_append(
            v.note, "backwards: frame " + std::to_string(i) + " asked, "
                        + (j >= 0 ? "frame " + std::to_string(j) : std::string("none"))
                        + " shown");
        failure = "REVERSE_WRONG_FRAME";
        v.status = failure;
        v.first_mismatch = int64_t(i);
        break;
      }
    }
  }
  if(failure && v.status.empty())
    v.status = failure;
  if(!failure)
    v.status = "OK";

  extra += sequential.json("seq") + scrub.json("scrub") + reverse.json("reverse");
  if(dec->fps > 0)
  {
    char buf[96];
    std::snprintf(
        buf, sizeof(buf), ",\"fps\":%.3f,\"realtime\":%s", dec->fps,
        sequential.count && sequential.total_ms / sequential.count < 1000. / dec->fps
            ? "true"
            : "false");
    extra += buf;
  }
  if(clk::now() >= deadline)
    note_append(v.note, "time budget reached");
  return finish();
}

// ---------------------------------------------------------------------------
// The frame queue's timing: VideoDecoder decoding ahead on its thread, and
// VideoFrameReader::nextFrame picking the frame for the node's time, as
// VideoNode::update does. Played two ticks per frame, then seeked at random.
// At every tick the frame on screen must be the one the direct-renderer check
// expects for that time: the last frame starting at or before it, from the
// same time 0 (playbackStartPts). Both renderers measured against one rule is
// what makes them agree.
// ---------------------------------------------------------------------------

int run_queue_timing(const std::string& path)
{
  constexpr const char* mode = "queue_timing";
  constexpr int max_ticks = 600;
  const auto deadline = clk::now() + playback_budget;
  Verdict v;

  auto dec = std::make_shared<Video::VideoDecoder>(Video::DecoderConfiguration{});
  if(!dec->load(path))
  {
    v.status = "SKIP";
    emit(mode, path, v);
    return 0;
  }
  const bool raw = !dec->m_conf.useAVCodec;
  auto ref = reference_decode(path, deadline, int(raw));
  v.ref_frames = ref.opened ? int64_t(ref.frames.size()) : -1;
  v.native_format = ref.native_format;
  if(!ref.opened || ref.frames.empty())
  {
    v.status = "SKIP";
    emit(mode, path, v);
    return 0;
  }
  const auto& frames = ref.frames;
  if(dec->frame_access == Video::FrameAccess::Sequential)
    note_append(v.note, "no timestamps: frames numbered at the frame rate");

  const double fps = dec->fps > 0. ? dec->fps : 24.;
  auto seconds_of = [&](int64_t pts) {
    return dec->flicks_per_dts * double(pts - dec->start_pts)
           / ossia::flicks_per_second<double>;
  };
  // The reference frame for a time: the last one starting at or before it,
  // the first one before any. The time is taken to the stream's nearest tick,
  // as both renderers take it.
  auto expected_at = [&](double t) {
    const int64_t now = Video::flicksToPts(
        int64_t(t * ossia::flicks_per_second<double>), dec->time_base, dec->start_pts);
    size_t best = 0;
    for(size_t i = 0; i < frames.size(); i++)
      if(frames[i].pts <= now)
        best = i;
    return best;
  };
  auto signature = [&](const AVFrame* f) -> FrameSig {
    if(raw)
      return {f->pts, uint32_t(av_adler32_update(1, f->data[0], f->linesize[0]))};
    return {f->pts, hash_frame_pixels(f)};
  };

  // The node's own reader, as VideoNode::update drives it: readNextFrame at
  // the node's time, then the frame it put on screen.
  score::gfx::VideoNode node{dec, std::nullopt};
  std::optional<FrameSig> shown;
  bool showing = false;
  // One tick at time t: the queue is given time to decode what is due, as a
  // real playback gives it a frame interval.
  auto tick = [&](double t) {
    node.standardUBO.time = t;
    for(int wait = 0; wait < 2000 && dec->m_frames.size() == 0 && !dec->m_finished
                      && clk::now() < deadline;
        wait++)
      std::this_thread::sleep_for(std::chrono::microseconds(500));
    node.reader.readNextFrame(node);
    std::lock_guard lock{node.reader.m_frameLock};
    if(auto& cur = node.reader.m_currentFrame; cur && cur->frame)
    {
      shown = signature(cur->frame);
      showing = true;
    }
  };

  int checked = 0, bad = 0;
  std::string first_bad;
  auto check = [&](double t, const char* phase) {
    if(!shown)
      return;
    checked++;
    const auto want = frames[expected_at(t)];
    const auto got = *shown;
    if(got.pts == want.pts && got.hash == want.hash)
      return;
    if(bad++ == 0)
    {
      size_t j = frames.size();
      for(size_t i = 0; i < frames.size(); i++)
        if(frames[i].pts == got.pts && frames[i].hash == got.hash)
          j = i;
      first_bad = std::string(phase) + " t=" + std::to_string(t) + ": frame "
                  + std::to_string(expected_at(t)) + " expected, "
                  + (j < frames.size() ? "frame " + std::to_string(j)
                                       : "pts " + std::to_string(got.pts))
                  + " shown";
    }
  };

  // In order. A frame is checked from its second tick: the first tick of a
  // frame is where a queue that has not decoded it yet legitimately lags.
  const double step = 0.5 / fps;
  const double end = std::min(
      seconds_of(frames.back().pts), (max_ticks / 2) / fps);
  const bool dump = getenv("VIDEO_TESTER_DUMP");
  for(double t = 0.; t <= end && clk::now() < deadline; t += step)
  {
    tick(t);
    tick(t);
    check(t, "playing");
    if(dump)
      std::fprintf(
          stderr, "t=%.4f expected %zu (pts %" PRId64 ") shown pts %" PRId64
                  " dur %" PRId64 " pending %" PRId64 " queued %zu\n",
          t, expected_at(t), frames[expected_at(t)].pts, shown ? shown->pts : int64_t(-1),
          int64_t(-1), int64_t(-1),
          size_t(dec->m_frames.size()));
  }
  const int bad_playing = bad;

  // Seeks: the queue gets a few ticks at the new time to show it.
  std::mt19937 rng{1234};
  std::uniform_int_distribution<size_t> pick{0, frames.size() - 1};
  for(int k = 0; k < 10 && clk::now() < deadline; k++)
  {
    const size_t i = pick(rng);
    const double t = std::max(0., seconds_of(frames[i].pts)) + 0.25 / fps;
    dec->seek(int64_t(t * ossia::flicks_per_second<double>));
    // The seek happens on the decoder's thread: real playback keeps ticking
    // meanwhile, a frame interval apart. Up to a second of those.
    const auto want = frames[expected_at(t)];
    for(int n = 0; n < int(fps) + 1; n++)
    {
      tick(t);
      if(dump)
        std::fprintf(
            stderr, "seek t=%.4f want pts %" PRId64 " shown %" PRId64
                    " queued %zu finished %d gen %d\n",
            t, want.pts, shown ? shown->pts : int64_t(-1), size_t(dec->m_frames.size()),
            int(dec->m_finished), dec->seek_generation.load());
      if(shown && shown->pts == want.pts)
        break;
      std::this_thread::sleep_for(std::chrono::microseconds(int64_t(1e6 / fps)));
    }
    check(t, "seek");
  }

  const int bad_seeking = bad - bad_playing;

  // Backwards, as score plays when its speed is negative: halfway into each
  // frame, last first. Each step back is a seek the reader asks the decoder
  // for: it gets a frame interval of ticks, up to a second, to show it.
  {
    const size_t last = std::min<size_t>(frames.size(), size_t(max_ticks / 2)) - 1;
    for(size_t k = 0; k < 30 && k <= last && clk::now() < deadline; k++)
    {
      const size_t i = last - k;
      const int64_t next
          = i + 1 < frames.size() ? frames[i + 1].pts : frames[i].pts + 1;
      const double t = seconds_of((frames[i].pts + next) / 2);
      for(int n = 0; n < int(fps) + 1; n++)
      {
        tick(t);
        if(shown && shown->pts == frames[i].pts)
          break;
        std::this_thread::sleep_for(std::chrono::microseconds(int64_t(1e6 / fps)));
      }
      check(t, "backwards");
    }
  }
  const int bad_reverse = bad - bad_playing - bad_seeking;

  v.score_frames = checked;
  v.status = !showing      ? (libav_decodes_a_frame(path) ? "NO_FRAMES" : "SKIP")
             : bad == 0     ? "OK"
             : bad_playing ? "TIMING_MISMATCH"
             : bad_seeking ? "SCRUB_TIMING_MISMATCH"
                            : "REVERSE_TIMING_MISMATCH";
  if(bad)
    note_append(
        v.note, std::to_string(bad) + "/" + std::to_string(checked) + " ticks wrong; "
                    + first_bad);
  if(clk::now() >= deadline)
    note_append(v.note, "time budget reached");
  emit(mode, path, v);
  return 0;
}
#endif

// ---------------------------------------------------------------------------
// Hardware decoding: same oracle, decoding through score's hwdec path.
// ---------------------------------------------------------------------------

struct HwAccelName
{
  const char* name;
  AVPixelFormat fmt;
};
constexpr HwAccelName hw_accels[] = {
    {"vaapi", AV_PIX_FMT_VAAPI},   {"vdpau", AV_PIX_FMT_VDPAU},
    {"cuda", AV_PIX_FMT_CUDA},     {"qsv", AV_PIX_FMT_QSV},
    {"vulkan", AV_PIX_FMT_VULKAN}, {"drm", AV_PIX_FMT_DRM_PRIME},
#if defined(__APPLE__)
    {"videotoolbox", AV_PIX_FMT_VIDEOTOOLBOX},
#endif
#if defined(_WIN32)
    {"d3d11va", AV_PIX_FMT_D3D11},
    {"d3d12va", AV_PIX_FMT_D3D12},
    {"dxva2", AV_PIX_FMT_DXVA2_VLD},
#endif
};

int run_hw(const std::string& path, const std::string& accel, AVPixelFormat hwfmt)
{
  const auto t0 = clk::now();
  const std::string mode = "hwdec:" + accel;
  Verdict v;
  v.requested = accel;

  // Software reference first: it defines both the expected frames and the
  // pixel format the comparison happens in. pts filtering is off on both
  // sides: conformance suites are raw streams whose frames carry no pts, and
  // hw validation is about decoders, not timing policy.
  auto ref = reference_decode(path, t0 + direct_budget, 0, /* ignore_pts: */ true);
  v.ref_frames = ref.opened ? int64_t(ref.frames.size()) : -1;
  v.ref_raw_frames = ref.opened ? ref.raw_frames : -1;
  v.native_format = ref.native_format;
  v.note = ref.note;

  if(!ref.opened || ref.frames.empty())
  {
    v.status = "SKIP";
    emit(mode.c_str(), path, v);
    return 0;
  }

  Video::DecoderConfiguration conf;
  conf.hardwareAcceleration = hwfmt;
  conf.ignorePTS = true;
  auto sc = score_decode_direct(path, t0 + direct_budget, conf, ref.out_fmt);
  v.score_frames = sc.opened ? int64_t(sc.frames.size()) : -1;
  v.out_format = sc.out_format;
  if(!sc.note.empty())
    note_append(v.note, sc.note);

  // What actually decoded. score substitutes another accel or falls back to
  // software when the requested one cannot handle this codec — the verdict
  // has to say which hardware, if any, was exercised.
  if(!sc.engaged_device.empty())
    v.engaged = sc.engaged_device;
  else if(sc.codec_name.find("v4l2m2m") != std::string::npos)
    v.engaged = sc.codec_name;
  else
    v.engaged = "sw";

  if(!sc.opened)
  {
    v.status = "SCORE_CANT_OPEN";
    emit(mode.c_str(), path, v);
    return 0;
  }
  if(sc.raw_gpu)
  {
    v.status = "NOT_APPLICABLE"; // HAP/DXV: packets go to the GPU undecoded
    emit(mode.c_str(), path, v);
    return 0;
  }
  if(v.engaged == "sw")
  {
    // Nothing engaged: the combination is unsupported on this machine (or
    // for this codec). Not a failure — but not a validation either.
    v.status = "HW_FALLBACK";
    emit(mode.c_str(), path, v);
    return 0;
  }
  if(v.engaged.find(accel) == std::string::npos
     && !(accel == "drm" && v.engaged.find("v4l2m2m") != std::string::npos))
    note_append(v.note, "substituted: " + v.engaged + " answered for " + accel);

  if(getenv("VIDEO_TESTER_DUMP"))
  {
    const size_t total = std::max(ref.frames.size(), sc.frames.size());
    for(size_t i = 0; i < total; i++)
    {
      auto fmt = [](const std::vector<FrameSig>& fs, size_t i) -> std::string {
        if(i >= fs.size())
          return "-";
        return std::to_string(fs[i].pts) + "/" + std::to_string(fs[i].hash);
      };
      std::fprintf(
          stderr, "%4zu ref %-24s score %-24s\n", i, fmt(ref.frames, i).c_str(),
          fmt(sc.frames, i).c_str());
    }
  }

  // Streams with no timestamps (raw conformance bitstreams) come out of the
  // generic decoder with AV_NOPTS_VALUE while the qsv/cuvid wrappers
  // synthesize timing — content is what hw validation compares, so pts only
  // counts when the reference actually has one.
  const size_t n = std::min(ref.frames.size(), sc.frames.size());
  size_t bad = n;
  for(size_t i = 0; i < n; i++)
  {
    const bool pts_bad = ref.frames[i].pts != INT64_MIN /* AV_NOPTS_VALUE */
                         && ref.frames[i].pts != sc.frames[i].pts;
    if(pts_bad || ref.frames[i].hash != sc.frames[i].hash)
    {
      bad = i;
      break;
    }
  }
  // Some hw decoders legitimately emit fewer frames than software: the Intel
  // runtime skips VP8 duplicate frames, corrupt conformance frames get
  // dropped instead of concealed... If everything the hw path did emit
  // matches a software frame at the same timestamp, that is a policy
  // difference, not corruption.
  auto matching_subsequence = [&]() -> bool {
    if(sc.frames.empty() || sc.frames.size() >= ref.frames.size())
      return false;
    size_t r = 0;
    for(const auto& s : sc.frames)
    {
      while(r < ref.frames.size()
            && !(ref.frames[r].hash == s.hash
                 && (ref.frames[r].pts == s.pts || ref.frames[r].pts == INT64_MIN)))
        r++;
      if(r == ref.frames.size())
        return false;
      r++;
    }
    return true;
  };

  if((bad < n || ref.frames.size() != sc.frames.size()) && matching_subsequence())
  {
    v.status = "HW_SKIPS_FRAMES";
    note_append(
        v.note, "hw emitted " + std::to_string(sc.frames.size()) + " of "
                    + std::to_string(ref.frames.size())
                    + " frames, every one matching sw at its pts");
  }
  else if(bad < n)
  {
    v.first_mismatch = int64_t(bad);
    if(ref.frames[bad].pts != INT64_MIN && ref.frames[bad].pts != sc.frames[bad].pts)
      v.status = "PTS_MISMATCH";
    else
    {
      g_capture = {};
      g_capture.index = int64_t(bad);
      const auto t1 = clk::now();
      score_decode_direct(path, t1 + direct_budget, conf, ref.out_fmt);
      reference_decode(path, t1 + direct_budget, 0, true);

      size_t diff_bytes = 0;
      int max_delta = 0;
      const size_t total = std::min(g_capture.ref.size(), g_capture.score.size());
      // On >8-bit formats a one-code sample step crosses a byte boundary and
      // reads as a delta of 255 byte-wise: measure sample values, not bytes.
      const auto* d16 = av_pix_fmt_desc_get(ref.out_fmt);
      const bool words = d16 && d16->comp[0].depth > 8 && total % 2 == 0;
      if(words)
      {
        for(size_t i = 0; i + 1 < total; i += 2)
        {
          const int a = g_capture.ref[i] | (g_capture.ref[i + 1] << 8);
          const int b = g_capture.score[i] | (g_capture.score[i + 1] << 8);
          const int d = std::abs(a - b);
          diff_bytes += (d != 0) * 2;
          max_delta = std::max(max_delta, d);
        }
      }
      else
      {
        for(size_t i = 0; i < total; i++)
        {
          const int d = std::abs(int(g_capture.ref[i]) - int(g_capture.score[i]));
          diff_bytes += d != 0;
          max_delta = std::max(max_delta, d);
        }
      }
      diff_bytes += std::max(g_capture.ref.size(), g_capture.score.size()) - total;

      note_append(
          v.note, "frame " + std::to_string(bad) + ": " + std::to_string(diff_bytes)
                      + "/" + std::to_string(total) + " bytes differ, max delta "
                      + std::to_string(max_delta));
      const bool minor = total > 0 && diff_bytes <= std::max<size_t>(64, total / 200);
      // H.264/HEVC/VP8/VP9/AV1 decoding is spec-exact — hardware must match
      // the software decoder bit for bit. MPEG-1/2/4 and friends allow
      // non-exact IDCTs: small-amplitude drift there is conformant.
      const bool drift = max_delta <= 3 && total > 0
                         && g_capture.ref.size() == g_capture.score.size();
      const bool damaged = g_av_diagnostics.load(std::memory_order_relaxed) > 0;
      v.status = minor      ? "PIXEL_MISMATCH_MINOR"
                 : drift    ? "PIXEL_DRIFT"
                 : damaged ? "PIXEL_MISMATCH_DAMAGED"
                           : "PIXEL_MISMATCH";
      g_capture = {};
    }
  }
  else if(ref.frames.size() != sc.frames.size())
    v.status = "COUNT_MISMATCH";
  else
    v.status = "OK";

  if(int nd = g_av_diagnostics.load(std::memory_order_relaxed))
    note_append(v.note, "libav diagnostics: " + std::to_string(nd));

  emit(mode.c_str(), path, v);
  return 0;
}

// ---------------------------------------------------------------------------
// Playback: the real threaded path, as the application runs it.
// ---------------------------------------------------------------------------

int run_playback(const std::string& path, bool seek_stress)
{
  const auto t0 = clk::now();
  const auto deadline = t0 + playback_budget;
  Verdict v;

  Video::VideoDecoder dec{Video::DecoderConfiguration{}};
  if(!dec.load(path))
  {
    v.status = "SKIP";
    emit(seek_stress ? "seek" : "playback", path, v);
    return 0;
  }

  const int64_t duration = dec.duration();
  const double seek_points[] = {0.5, 0.1, 0.9};
  size_t next_seek = 0;
  int64_t frames = 0;
  int64_t frames_at_last_seek = 0;
  int idle_after_finish = 0;

  using namespace std::chrono_literals;
  while(clk::now() < deadline)
  {
    if(AVFrame* f = dec.dequeue_frame())
    {
      frames++;
      dec.release_frame(f);
      idle_after_finish = 0;

      if(seek_stress && duration > 0 && next_seek < std::size(seek_points)
         && frames - frames_at_last_seek >= 25)
      {
        dec.seek(int64_t(duration * seek_points[next_seek]));
        next_seek++;
        frames_at_last_seek = frames;
      }
    }
    else
    {
      if(dec.m_finished && ++idle_after_finish > 200)
        break;
      std::this_thread::sleep_for(1ms);
    }
  }

  v.score_frames = frames;
  if(clk::now() >= deadline && !(dec.m_finished))
    v.status = "TIMEOUT_INTERNAL";
  else if(frames == 0 && libav_decodes_a_frame(path))
    v.status = "NO_FRAMES";
  else
    v.status = "OK";
  if(seek_stress && next_seek < std::size(seek_points) && duration > 0)
    note_append(
        v.note, "only " + std::to_string(next_seek) + "/3 seeks exercised (short file)");

  emit(seek_stress ? "seek" : "playback", path, v);
  return 0;
}

}

int main(int argc, char** argv)
{
  bool playback = false, seek_stress = false, direct_renderer = false,
       queue_timing = false;
  std::string file, hwaccel;
  for(int i = 1; i < argc; i++)
  {
    std::string a = argv[i];
    if(a == "--playback")
      playback = true;
    else if(a == "--seek-stress")
      seek_stress = true;
    else if(a == "--direct-renderer")
      direct_renderer = true;
    else if(a == "--queue-timing")
      queue_timing = true;
    else if(a == "--hwaccel" && i + 1 < argc)
      hwaccel = argv[++i];
    else
      file = a;
  }
  if(file.empty())
  {
    std::fprintf(
        stderr,
        "usage: %s [--playback|--seek-stress|--direct-renderer|--queue-timing|"
        "--hwaccel <name>] "
        "<file>\n",
        argv[0]);
    return 2;
  }

  // FATE is full of deliberately broken files: swallow libav's chatter, but
  // count warnings and errors — a diverging frame in a file the decoder
  // complained about is concealment, not necessarily a decode-loop bug.
  av_log_set_callback(counting_log_cb);

  if(!hwaccel.empty())
  {
    for(const auto& a : hw_accels)
      if(hwaccel == a.name)
        return run_hw(file, hwaccel, a.fmt);
    std::fprintf(stderr, "unknown hwaccel: %s\n", hwaccel.c_str());
    return 2;
  }
  if(direct_renderer || queue_timing)
  {
#if SCORE_CORPUS_HAS_GFX
    return queue_timing ? run_queue_timing(file) : run_direct_renderer(file);
#else
    std::fprintf(stderr, "built without score-plugin-gfx: no --direct-renderer\n");
    return 2;
#endif
  }
  if(playback || seek_stress)
    return run_playback(file, seek_stress);
  return run_direct(file);
}

#else
int main()
{
  return 2;
}
#endif
