#pragma once
#include <score_plugin_recording_export.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace Recording
{
/**
 * Writes a WAV file as its frames come, for takes whose length is not known
 * when they start.
 *
 * The header reserves a JUNK chunk the size of RF64's ds64 chunk. At close,
 * the sizes are written in; a take that went past what a RIFF file can hold
 * (4 GB) becomes an RF64 file, the JUNK chunk its ds64 chunk, so shorter takes
 * stay plain WAV files that every program reads.
 *
 * Not real-time: it is meant for a writer thread.
 */
class SCORE_PLUGIN_RECORDING_EXPORT WavStreamWriter
{
public:
  enum class Format
  {
    Float32,
    Pcm24,
    Pcm16
  };

  WavStreamWriter() = default;
  ~WavStreamWriter();
  WavStreamWriter(const WavStreamWriter&) = delete;
  WavStreamWriter& operator=(const WavStreamWriter&) = delete;

  //! Creates (or truncates) the file and writes a provisional header
  bool open(const std::string& path, int channels, int rate, Format format);

  //! Appends frames given one channel after the other
  bool write(const float* const* channels, int64_t frames);

  //! Writes the sizes in; the file is complete afterwards
  bool close();

  bool isOpen() const noexcept { return m_file; }
  bool failed() const noexcept { return m_failed; }
  int64_t frames() const noexcept { return m_frames; }
  int channels() const noexcept { return m_channels; }
  int rate() const noexcept { return m_rate; }

  //! The size past which the file becomes RF64; lowered by tests only.
  void setRiffLimit(uint64_t bytes) noexcept { m_riffLimit = bytes; }

private:
  int bytesPerSample() const noexcept;

  std::FILE* m_file{};
  std::vector<unsigned char> m_buffer;
  uint64_t m_riffLimit{0xFFFFFFFFull};
  int64_t m_frames{};
  int64_t m_dataStart{};
  int m_channels{};
  int m_rate{};
  Format m_format{Format::Float32};
  bool m_failed{};
};
}
