#pragma once
#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV

#include <Video/ExternalInput.hpp>
#include <Video/FrameQueue.hpp>
#include <Video/Rescale.hpp>

extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

#include <score_plugin_media_export.h>

#include <atomic>
#include <chrono>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace Video
{

// Lock-free ring buffer for audio, shared between decoder thread and audio engine.
// Same design as Gfx::GStreamer::gstreamer_pipeline::AudioBuffer.
// FIXME use a proper one, not this shit.
struct SCORE_PLUGIN_MEDIA_EXPORT AudioRingBuffer
{
  int sample_rate{48000};
  int num_channels{2};

  static constexpr std::size_t ring_size = 65536;
  std::vector<std::vector<float>> ring; // [channel][ring_size]
  std::atomic<std::size_t> write_pos{0};
  std::atomic<std::size_t> read_pos{0};

  // Set by the audio parameter to point at its backing storage
  std::vector<ossia::float_vector>* output_data{};

  void init(int nchannels);
  void write_planar(float** data, int num_samples, int channels);
  void write_interleaved_float(const float* data, int num_samples, int channels);
  void write_interleaved_s16(const int16_t* data, int num_samples, int channels);
  void read_into_output(int block_size);
};

// Demuxes any URL/file/device via avformat, decoding both video and audio.
// Video frames go into a FrameQueue (standard ExternalInput pattern).
// Audio samples go into an AudioRingBuffer for the audio engine.
class SCORE_PLUGIN_MEDIA_EXPORT LibavStreamInput final
    : public ExternalInput
    , public LibAVDecoder
{
public:
  LibavStreamInput() noexcept;
  ~LibavStreamInput() noexcept;

  bool load(const std::string& url) noexcept;
  bool load(
      const std::string& url,
      const std::map<std::string, std::string>& options) noexcept;

  // Open the format context and discover streams (populates width/height/etc).
  // Keeps the context alive for later start().
  bool probe() noexcept;

  bool start() noexcept override;
  void stop() noexcept override;

  AVFrame* dequeue_frame() noexcept override;
  void release_frame(AVFrame* frame) noexcept override;

  bool has_audio() const noexcept { return m_audioStream != nullptr; }
  AudioRingBuffer& audio_buffer() noexcept { return m_audioBuf; }
  const std::string& url() const noexcept { return m_url; }

private:
  void buffer_thread() noexcept;
  void close_file() noexcept;
  bool open_streams() noexcept;
  void close_streams() noexcept;
  void decode_audio_packet(AVPacket& packet) noexcept;

  std::string m_url;
  std::map<std::string, std::string> m_options;

  // Audio decoding state
  AVStream* m_audioStream{};
  const AVCodec* m_audioCodec{};
  AVCodecContext* m_audioCodecContext{};
  AVFrame* m_audioFrame{};
  AudioRingBuffer m_audioBuf;

  std::thread m_thread;
  std::atomic_bool m_running{};

  // Looping: -1 = infinite, 0 = no loop, >0 = N remaining loops.
  // Extracted from the "loop" key in options (not passed to FFmpeg).
  int m_loop{0};

  // True for file-based sources that need producer-side frame pacing.
  // False for real-time sources (network, devices, lavfi) where I/O naturally paces.
  bool m_needsPacing{false};
};

}
#endif
