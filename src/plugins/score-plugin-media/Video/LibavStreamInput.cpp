#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV
#include "LibavStreamInput.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
}

#include <Audio/AudioTick.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/thread.hpp>

#include <QDebug>

#include <string_view>

namespace Video
{

// AudioRingBuffer implementation

void AudioRingBuffer::init(int nchannels)
{
  num_channels = nchannels;
  ring.resize(nchannels);
  for(auto& ch : ring)
    ch.resize(ring_size, 0.f);
  write_pos = 0;
  read_pos = 0;
}

void AudioRingBuffer::write_planar(float** data, int num_samples, int channels)
{
  int nch = std::min(channels, num_channels);
  auto wp = write_pos.load(std::memory_order_relaxed);
  for(int ch = 0; ch < nch; ch++)
  {
    for(int s = 0; s < num_samples; s++)
      ring[ch][(wp + s) % ring_size] = data[ch][s];
  }
  write_pos.store(wp + num_samples, std::memory_order_release);
}

void AudioRingBuffer::write_interleaved_float(
    const float* data, int num_samples, int channels)
{
  int nch = std::min(channels, num_channels);
  auto wp = write_pos.load(std::memory_order_relaxed);
  for(int s = 0; s < num_samples; s++)
  {
    for(int ch = 0; ch < nch; ch++)
      ring[ch][(wp + s) % ring_size] = data[s * channels + ch];
  }
  write_pos.store(wp + num_samples, std::memory_order_release);
}

void AudioRingBuffer::write_interleaved_s16(
    const int16_t* data, int num_samples, int channels)
{
  int nch = std::min(channels, num_channels);
  auto wp = write_pos.load(std::memory_order_relaxed);
  for(int s = 0; s < num_samples; s++)
  {
    for(int ch = 0; ch < nch; ch++)
      ring[ch][(wp + s) % ring_size] = data[s * channels + ch] / 32768.f;
  }
  write_pos.store(wp + num_samples, std::memory_order_release);
}

void AudioRingBuffer::read_into_output(int block_size)
{
  if(!output_data)
    return;
  auto rp = read_pos.load(std::memory_order_relaxed);
  auto wp = write_pos.load(std::memory_order_acquire);

  std::size_t available = (wp >= rp) ? (wp - rp) : 0;

  int nch = std::min((int)output_data->size(), num_channels);
  if(available >= (std::size_t)block_size)
  {
    for(int ch = 0; ch < nch; ch++)
    {
      auto& dst = (*output_data)[ch];
      auto& src = ring[ch];
      for(int s = 0; s < block_size; s++)
        dst[s] = src[(rp + s) % ring_size];
    }
    read_pos.store(rp + block_size, std::memory_order_release);
  }
  else
  {
    // Underrun: output silence
    for(int ch = 0; ch < nch; ch++)
      std::fill_n((*output_data)[ch].data(), block_size, 0.f);
  }
}

// LibavStreamInput implementation

LibavStreamInput::LibavStreamInput() noexcept
{
  realTime = true;
}

LibavStreamInput::~LibavStreamInput() noexcept
{
  close_file();
}

bool LibavStreamInput::load(const std::string& url) noexcept
{
  close_file();
  m_url = url;
  m_options.clear();
  m_conf.ignorePTS = true;
  return !url.empty();
}

bool LibavStreamInput::load(
    const std::string& url,
    const std::map<std::string, std::string>& options) noexcept
{
  close_file();
  m_url = url;
  m_options = options;
  m_conf.ignorePTS = true;
  return !url.empty();
}

bool LibavStreamInput::probe() noexcept
{
  if(m_formatContext)
    return true; // Already probed

  m_formatContext = avformat_alloc_context();
  if(!m_formatContext)
    return false;

  AVDictionary* options = nullptr;
  const AVInputFormat* input_fmt = nullptr;

  if(m_options.empty())
  {
    // Default: low-latency flags for network streams
    m_formatContext->flags |= AVFMT_FLAG_NOBUFFER;
    m_formatContext->flags |= AVFMT_FLAG_FLUSH_PACKETS;
    av_dict_set(&options, "fflags", "nobuffer", 0);
    av_dict_set(&options, "flags", "low_delay", 0);
  }
  else
  {
    // User-provided options: extract keys handled at our level,
    // pass the rest as AVDictionary to the demuxer.
    for(const auto& [k, v] : m_options)
    {
      if(k == "format")
        input_fmt = av_find_input_format(v.c_str());
      else if(k == "loop")
      {
        // loop: -1 = infinite, 0 = no loop, N = loop N times.
        // This is NOT an FFmpeg option — we implement it ourselves
        // by seeking back to start on EOF (same as ffmpeg CLI's -stream_loop).
        try
        {
          m_loop = std::stoi(v);
        }
        catch(...)
        {
          m_loop = 0;
        }
      }
      else
        av_dict_set(&options, k.c_str(), v.c_str(), 0);
    }
  }

  int ret = avformat_open_input(&m_formatContext, m_url.c_str(), input_fmt, &options);
  av_dict_free(&options);

  if(ret < 0)
  {
    qDebug() << "FFmpeg input: avformat_open_input failed for"
             << m_url.c_str() << av_to_string(ret);
    m_formatContext = nullptr; // avformat_open_input frees on failure
    return false;
  }

  ret = avformat_find_stream_info(m_formatContext, nullptr);
  if(ret < 0)
  {
    qDebug() << "FFmpeg input: avformat_find_stream_info failed:"
             << av_to_string(ret);
    avformat_close_input(&m_formatContext);
    m_formatContext = nullptr;
    return false;
  }

  if(!open_streams())
  {
    qDebug() << "FFmpeg input: no usable streams found";
    close_streams();
    avformat_close_input(&m_formatContext);
    m_formatContext = nullptr;
    return false;
  }

  // Determine if this source needs producer-side frame pacing.
  // Real-time capture devices are paced by their hardware clock.
  // Network streams are paced by I/O blocking (av_read_frame waits for data).
  // Everything else (files, image sequences, lavfi generators) delivers
  // frames as fast as possible and needs explicit pacing.
  m_needsPacing = false;
  if(fps > 0 && m_formatContext->iformat)
  {
    // Special-case some devices where pacing is useful:
    using namespace std::literals;
    if(m_formatContext->iformat->name == "lavfi"sv)
    {
      m_needsPacing = true;
    }
    else
    {
      bool isNetwork = m_url.find("://") != std::string::npos;

      // Check if the demuxer is a registered capture device (v4l2, x11grab, etc.)
      // using FFmpeg's own device registry.
      bool isDevice = false;
      {
        const AVInputFormat* d = nullptr;
        while((d = av_input_video_device_next(d)))
          if(d == m_formatContext->iformat)
          {
            isDevice = true;
            break;
          }
        if(!isDevice)
          while((d = av_input_audio_device_next(d)))
            if(d == m_formatContext->iformat)
            {
              isDevice = true;
              break;
            }
      }

      m_needsPacing = !isNetwork && !isDevice;
    }
  }

  return true;
}

bool LibavStreamInput::start() noexcept
{
  if(m_running)
    return false;

  // If not yet probed, do it now
  if(!m_formatContext)
  {
    if(!probe())
      return false;
  }

  m_running.store(true, std::memory_order_release);
  m_thread = std::thread{[this] {
    ossia::set_thread_name("ossia ffmpeg input");
    this->buffer_thread();
  }};
  return true;
}

void LibavStreamInput::stop() noexcept
{
  close_file();
}

AVFrame* LibavStreamInput::dequeue_frame() noexcept
{
  return m_frames.dequeue();
}

void LibavStreamInput::release_frame(AVFrame* frame) noexcept
{
  m_frames.release(frame);
}

void LibavStreamInput::buffer_thread() noexcept
{
  AVPacket packet;
  memset(&packet, 0, sizeof(AVPacket));

  // Pacing state for file-based sources.
  // When the ossia execution engine is running, sync to the audio clock
  // (Audio::execution_samples). Otherwise, use a wall-clock fallback.
  using clock = std::chrono::steady_clock;
  const auto frame_duration
      = m_needsPacing ? std::chrono::microseconds(int64_t(1000000.0 / fps))
                      : std::chrono::microseconds(0);
  auto next_frame_time = clock::now();
  int64_t video_frame_count = 0;

  while(m_running.load(std::memory_order_acquire))
  {
    int ret = av_read_frame(m_formatContext, &packet);
    if(ret < 0)
    {
      if(ret == AVERROR_EOF)
      {
        // End of file: loop if requested
        if(m_loop != 0)
        {
          // Flush decoders before seeking
          if(m_codecContext)
            avcodec_flush_buffers(m_codecContext);
          if(m_audioCodecContext)
            avcodec_flush_buffers(m_audioCodecContext);

          // Seek back to start
          int64_t start = m_formatContext->start_time;
          if(start == AV_NOPTS_VALUE)
            start = 0;
          avformat_seek_file(m_formatContext, -1, INT64_MIN, start, start, 0);

          if(m_loop > 0)
            m_loop--;

          video_frame_count = 0;
          next_frame_time = clock::now();
          continue;
        }
        break;
      }
      if(ret == AVERROR(EAGAIN))
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      // Actual error
      break;
    }

    if(m_avstream && packet.stream_index == m_avstream->index)
    {
      // Use enqueue_frame (send_packet + receive_frame) — NOT read_one_frame_avcodec,
      // which internally calls av_read_frame again, causing double-reads and
      // lost packets that break H.264/H.265 reference frame chains.
      ReadFrame res = enqueue_frame(&packet);
      if(res.frame)
      {
        if(m_frames.size() < 8)
          m_frames.enqueue(res.frame);
        else
          m_frames.release(res.frame);
      }

      video_frame_count++;

      // Pace file-based sources to prevent the producer from running ahead.
      if(m_needsPacing)
      {
        const bool playing
            = Audio::execution_status.load(std::memory_order_relaxed)
              == ossia::transport_status::playing;

        if(playing)
        {
          // Sync to the audio clock: wait until enough audio samples have
          // elapsed for the next video frame. Lock-free, drift-free.
          const int sr = Audio::execution_sample_rate.load(std::memory_order_relaxed);
          const int64_t target_samples = int64_t(video_frame_count * sr / fps);

          while(m_running.load(std::memory_order_acquire))
          {
            const int64_t cur
                = Audio::execution_samples.load(std::memory_order_acquire);
            if(cur >= target_samples)
              break;
            // Sleep for roughly half the remaining time
            const int64_t remaining = target_samples - cur;
            const auto us = remaining * 1000000 / sr;
            if(us > 1000)
              std::this_thread::sleep_for(std::chrono::microseconds(us / 2));
            else
              std::this_thread::yield();
          }
        }
        else
        {
          // Not playing — wall-clock fallback for preview without execution
          next_frame_time += frame_duration;
          std::this_thread::sleep_until(next_frame_time);
        }
      }
    }
    else if(m_audioStream && packet.stream_index == m_audioStream->index)
    {
      // Audio packet — decode and write to ring buffer
      decode_audio_packet(packet);
    }

    av_packet_unref(&packet);
  }
}

void LibavStreamInput::close_file() noexcept
{
  m_running.store(false, std::memory_order_release);

  if(m_thread.joinable())
    m_thread.join();

  m_frames.drain();

  close_streams();

  if(m_formatContext)
  {
    avformat_close_input(&m_formatContext);
    m_formatContext = nullptr;
  }

  m_rescale.close();
}

bool LibavStreamInput::open_streams() noexcept
{
  if(!m_formatContext)
    return false;

  bool found_video = false;

  for(unsigned int i = 0; i < m_formatContext->nb_streams; i++)
  {
    auto stream = m_formatContext->streams[i];
    auto codecPar = stream->codecpar;
    if(!codecPar)
      continue;

    if(codecPar->codec_type == AVMEDIA_TYPE_VIDEO && !found_video)
    {
      m_codec = avcodec_find_decoder(codecPar->codec_id);
      if(m_codec)
      {
        m_avstream = stream;
        pixel_format = static_cast<AVPixelFormat>(codecPar->format);
        width = codecPar->width;
        height = codecPar->height;
        if(stream->avg_frame_rate.num != 0 && stream->avg_frame_rate.den != 0)
          fps = av_q2d(stream->avg_frame_rate);

        found_video = open_codec_context(
            *this, stream,
            [this, stream](AVCodecContext& ctx) {
              ctx.framerate
                  = av_guess_frame_rate(m_formatContext, (AVStream*)stream, NULL);
              m_codecContext->flags |= AV_CODEC_FLAG_LOW_DELAY;
              m_codecContext->flags2 |= AV_CODEC_FLAG2_FAST;
            });
      }
    }
    else if(codecPar->codec_type == AVMEDIA_TYPE_AUDIO && !m_audioStream)
    {
      m_audioCodec = avcodec_find_decoder(codecPar->codec_id);
      if(m_audioCodec)
      {
        m_audioCodecContext = avcodec_alloc_context3(m_audioCodec);
        if(m_audioCodecContext)
        {
          avcodec_parameters_to_context(m_audioCodecContext, codecPar);
          if(avcodec_open2(m_audioCodecContext, m_audioCodec, nullptr) >= 0)
          {
            m_audioStream = stream;
            m_audioFrame = av_frame_alloc();

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
            int nch = m_audioCodecContext->ch_layout.nb_channels;
#else
            int nch = m_audioCodecContext->channels;
#endif
            m_audioBuf.sample_rate = m_audioCodecContext->sample_rate;
            m_audioBuf.init(nch > 0 ? nch : 2);
          }
          else
          {
            avcodec_free_context(&m_audioCodecContext);
          }
        }
      }
    }
  }

  // At least one stream must be found
  return found_video || m_audioStream;
}

void LibavStreamInput::close_streams() noexcept
{
  // Video
  if(m_codecContext)
    avcodec_free_context(&m_codecContext);
  m_codecContext = nullptr;
  m_codec = nullptr;
  m_avstream = nullptr;

  // Audio
  if(m_audioFrame)
    av_frame_free(&m_audioFrame);
  if(m_audioCodecContext)
    avcodec_free_context(&m_audioCodecContext);
  m_audioCodecContext = nullptr;
  m_audioCodec = nullptr;
  m_audioStream = nullptr;

  m_rescale.close();
}

void LibavStreamInput::decode_audio_packet(AVPacket& packet) noexcept
{
  if(!m_audioCodecContext || !m_audioFrame)
    return;

  int ret = avcodec_send_packet(m_audioCodecContext, &packet);
  if(ret < 0)
    return;

  while(ret >= 0)
  {
    ret = avcodec_receive_frame(m_audioCodecContext, m_audioFrame);
    if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      break;
    if(ret < 0)
      break;

    int num_samples = m_audioFrame->nb_samples;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
    int channels = m_audioFrame->ch_layout.nb_channels;
#else
    int channels = m_audioFrame->channels;
#endif

    AVSampleFormat fmt = (AVSampleFormat)m_audioFrame->format;

    switch(fmt)
    {
      case AV_SAMPLE_FMT_FLTP:
        m_audioBuf.write_planar(
            (float**)m_audioFrame->data, num_samples, channels);
        break;
      case AV_SAMPLE_FMT_FLT:
        m_audioBuf.write_interleaved_float(
            (const float*)m_audioFrame->data[0], num_samples, channels);
        break;
      case AV_SAMPLE_FMT_S16:
        m_audioBuf.write_interleaved_s16(
            (const int16_t*)m_audioFrame->data[0], num_samples, channels);
        break;
      case AV_SAMPLE_FMT_S16P: {
        // Convert planar S16 to float ring
        auto wp = m_audioBuf.write_pos.load(std::memory_order_relaxed);
        int nch = std::min(channels, m_audioBuf.num_channels);
        for(int ch = 0; ch < nch; ch++)
        {
          const int16_t* src = (const int16_t*)m_audioFrame->data[ch];
          for(int s = 0; s < num_samples; s++)
            m_audioBuf.ring[ch][(wp + s) % AudioRingBuffer::ring_size]
                = src[s] / 32768.f;
        }
        m_audioBuf.write_pos.store(
            wp + num_samples, std::memory_order_release);
        break;
      }
      default:
        // For other formats, skip (could add S32, DBL, etc. later)
        break;
    }
  }
}

}
#endif
