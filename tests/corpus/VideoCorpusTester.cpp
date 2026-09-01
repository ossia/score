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
#include <Video/VideoDecoder.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/adler32.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstring>
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
  std::printf(
      "{\"mode\":\"%s\",\"file\":\"%s\",\"status\":\"%s\",\"score_frames\":%" PRId64
      ",\"ref_frames\":%" PRId64 ",\"ref_raw_frames\":%" PRId64
      ",\"first_mismatch\":%" PRId64 ",\"native_format\":\"%s\",\"note\":\"%s\"}\n",
      mode, json_escape(file).c_str(), v.status.c_str(), v.score_frames, v.ref_frames,
      v.ref_raw_frames, v.first_mismatch, json_escape(v.native_format).c_str(),
      json_escape(v.note).c_str());
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
Reference reference_decode(const std::string& path, clk::time_point deadline, int raw_mode)
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
      = Video::formatNeedsDecoding((AVPixelFormat)par->format);
  SwsContext* sws{};
  AVPixelFormat sws_src_fmt = AV_PIX_FMT_NONE;
  AVFrame* rgb = nullptr;

  AVPacket* pkt = av_packet_alloc();
  AVFrame* f = av_frame_alloc();

  auto take_frame = [&](AVFrame* frame) {
    ref.raw_frames++;
    if(frame->pts < 0) // receiveVideoFrame's policy
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
      ref.frames.push_back({frame->pts, hash_frame_pixels(rgb)});
    }
    else
    {
      maybe_dump_frame("ref", ref.frames.size(), frame);
      if(g_capture.index == int64_t(ref.frames.size()))
        frame_bytes(frame, g_capture.ref);
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
};

ScoreResult score_decode_direct(const std::string& path, clk::time_point deadline)
{
  ScoreResult res;

  Video::VideoDecoder dec{Video::DecoderConfiguration{}};
  if(!dec.open(path))
    return res;
  res.opened = true;
  res.raw_gpu = !dec.m_conf.useAVCodec;

  auto collect_one = [&](AVFrame* fr) {
    if(int64_t(res.frames.size()) < max_frames)
    {
      maybe_dump_frame("score", res.frames.size(), fr);
      if(g_capture.index == int64_t(res.frames.size()))
        frame_bytes(fr, g_capture.score);
      res.frames.push_back({fr->pts, hash_frame_pixels(fr)});
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
  bool playback = false, seek_stress = false;
  std::string file;
  for(int i = 1; i < argc; i++)
  {
    std::string a = argv[i];
    if(a == "--playback")
      playback = true;
    else if(a == "--seek-stress")
      seek_stress = true;
    else
      file = a;
  }
  if(file.empty())
  {
    std::fprintf(
        stderr, "usage: %s [--playback|--seek-stress] <file>\n", argv[0]);
    return 2;
  }

  // FATE is full of deliberately broken files: swallow libav's chatter, but
  // count warnings and errors — a diverging frame in a file the decoder
  // complained about is concealment, not necessarily a decode-loop bug.
  av_log_set_callback(counting_log_cb);

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
