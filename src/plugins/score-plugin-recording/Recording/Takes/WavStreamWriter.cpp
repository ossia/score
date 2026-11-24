#include "WavStreamWriter.hpp"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Recording
{
namespace
{
// Little-endian writers into a byte buffer
void put16(unsigned char* p, uint16_t v)
{
  p[0] = v & 0xff;
  p[1] = (v >> 8) & 0xff;
}
void put32(unsigned char* p, uint32_t v)
{
  for(int i = 0; i < 4; i++)
    p[i] = (v >> (8 * i)) & 0xff;
}
void put64(unsigned char* p, uint64_t v)
{
  for(int i = 0; i < 8; i++)
    p[i] = (v >> (8 * i)) & 0xff;
}

// KSDATAFORMAT_SUBTYPE_PCM / _IEEE_FLOAT
constexpr unsigned char guid_tail[14]
    = {0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};

// RIFF / RF64 header (12) + JUNK or ds64 chunk (8 + 28) + fmt chunk + data
// chunk header. The fmt chunk is 16 bytes, or 40 with WAVE_FORMAT_EXTENSIBLE.
constexpr int junk_payload = 28;
constexpr int acid_size = 24;

bool seek(std::FILE* f, int64_t pos)
{
#if defined(_WIN32)
  return _fseeki64(f, pos, SEEK_SET) == 0;
#else
  return fseeko(f, pos, SEEK_SET) == 0;
#endif
}
int64_t tell(std::FILE* f)
{
#if defined(_WIN32)
  return _ftelli64(f);
#else
  return ftello(f);
#endif
}
}

WavStreamWriter::~WavStreamWriter()
{
  if(m_file)
    close();
}

int WavStreamWriter::bytesPerSample() const noexcept
{
  switch(m_format)
  {
    case Format::Float32:
      return 4;
    case Format::Pcm24:
      return 3;
    case Format::Pcm16:
      return 2;
  }
  return 4;
}

bool WavStreamWriter::open(
    const std::string& path, int channels, int rate, Format format)
{
  if(m_file)
    close();
  m_failed = false;
  m_frames = 0;
  m_channels = std::max(1, channels);
  m_rate = std::max(1, rate);
  m_format = format;

#if defined(_WIN32)
  // UTF-8 paths
  std::wstring wpath(path.size(), L'\0');
  const int n = MultiByteToWideChar(CP_UTF8, 0, path.data(), int(path.size()), wpath.data(), int(wpath.size()));
  wpath.resize(std::max(0, n));
  m_file = _wfopen(wpath.c_str(), L"wb");
#else
  m_file = std::fopen(path.c_str(), "wb");
#endif
  if(!m_file)
  {
    m_failed = true;
    return false;
  }

  const bool is_float = format == Format::Float32;
  const bool extensible = m_channels > 2 || format == Format::Pcm24;
  const int bps = bytesPerSample();
  const int block_align = bps * m_channels;
  const int fmt_size = extensible ? 40 : 16;

  // A second JUNK chunk, before the data where readers look for it, becomes
  // the ACID chunk at close
  std::vector<unsigned char> h(12 + 8 + junk_payload + 8 + fmt_size + 8 + acid_size + 8, 0);
  unsigned char* p = h.data();
  std::memcpy(p, "RIFF", 4);
  // size: at close
  std::memcpy(p + 8, "WAVE", 4);
  p += 12;
  std::memcpy(p, "JUNK", 4);
  put32(p + 4, junk_payload);
  p += 8 + junk_payload;
  std::memcpy(p, "fmt ", 4);
  put32(p + 4, fmt_size);
  put16(p + 8, extensible ? 0xFFFE : (is_float ? 3 : 1));
  put16(p + 10, uint16_t(m_channels));
  put32(p + 12, uint32_t(m_rate));
  put32(p + 16, uint32_t(m_rate * block_align));
  put16(p + 20, uint16_t(block_align));
  put16(p + 22, uint16_t(bps * 8));
  if(extensible)
  {
    put16(p + 24, 22);
    put16(p + 26, uint16_t(bps * 8));
    put32(p + 28, 0); // channel mask: unspecified
    put16(p + 32, is_float ? 3 : 1);
    std::memcpy(p + 34, guid_tail, 14);
  }
  p += 8 + fmt_size;
  m_acidStart = p - h.data();
  std::memcpy(p, "JUNK", 4);
  put32(p + 4, acid_size);
  p += 8 + acid_size;
  std::memcpy(p, "data", 4);
  // size: at close

  if(std::fwrite(h.data(), 1, h.size(), m_file) != h.size())
  {
    m_failed = true;
    return false;
  }
  m_dataStart = int64_t(h.size());
  return true;
}

bool WavStreamWriter::write(const float* const* channels, int64_t frames)
{
  if(!m_file || m_failed || frames <= 0)
    return !m_failed;

  const int bps = bytesPerSample();
  const std::size_t bytes = std::size_t(frames) * m_channels * bps;
  if(m_buffer.size() < bytes)
    m_buffer.resize(bytes);

  unsigned char* out = m_buffer.data();
  for(int64_t i = 0; i < frames; i++)
  {
    for(int c = 0; c < m_channels; c++)
    {
      const float v = channels[c] ? channels[c][i] : 0.f;
      switch(m_format)
      {
        case Format::Float32:
          std::memcpy(out, &v, 4);
          break;
        case Format::Pcm24: {
          const double clamped = std::clamp(double(v), -1., 1.);
          const int32_t s = int32_t(std::lround(clamped * 8388607.));
          out[0] = s & 0xff;
          out[1] = (s >> 8) & 0xff;
          out[2] = (s >> 16) & 0xff;
          break;
        }
        case Format::Pcm16: {
          const double clamped = std::clamp(double(v), -1., 1.);
          const int16_t s = int16_t(std::lround(clamped * 32767.));
          put16(out, uint16_t(s));
          break;
        }
      }
      out += bps;
    }
  }

  if(std::fwrite(m_buffer.data(), 1, bytes, m_file) != bytes)
  {
    m_failed = true;
    return false;
  }
  m_frames += frames;
  return true;
}

bool WavStreamWriter::close()
{
  if(!m_file)
    return !m_failed;

  bool ok = !m_failed;
  const uint64_t data_bytes = uint64_t(m_frames) * m_channels * bytesPerSample();

  // A chunk's size is even: an odd data size gets a pad byte
  if(data_bytes % 2)
  {
    const unsigned char pad = 0;
    ok = ok && std::fwrite(&pad, 1, 1, m_file) == 1;
  }
  const int64_t end = tell(m_file);
  const uint64_t riff_size = uint64_t(end) - 8;

  unsigned char b8[8];
  if(riff_size <= m_riffLimit && data_bytes <= m_riffLimit)
  {
    put32(b8, uint32_t(riff_size));
    ok = ok && seek(m_file, 4) && std::fwrite(b8, 1, 4, m_file) == 4;
    put32(b8, uint32_t(data_bytes));
    ok = ok && seek(m_file, m_dataStart - 4) && std::fwrite(b8, 1, 4, m_file) == 4;
  }
  else
  {
    // RF64: the sizes go in the ds64 chunk, the 32-bit fields say so
    ok = ok && seek(m_file, 0) && std::fwrite("RF64", 1, 4, m_file) == 4;
    put32(b8, 0xFFFFFFFFu);
    ok = ok && std::fwrite(b8, 1, 4, m_file) == 4;

    unsigned char ds64[8 + junk_payload]{};
    std::memcpy(ds64, "ds64", 4);
    put32(ds64 + 4, junk_payload);
    put64(ds64 + 8, riff_size);
    put64(ds64 + 16, data_bytes);
    put64(ds64 + 24, uint64_t(m_frames));
    put32(ds64 + 32, 0); // no table
    ok = ok && seek(m_file, 12) && std::fwrite(ds64, 1, sizeof(ds64), m_file) == sizeof(ds64);

    put32(b8, 0xFFFFFFFFu);
    ok = ok && seek(m_file, m_dataStart - 4) && std::fwrite(b8, 1, 4, m_file) == 4;
  }

  if(m_acid.tempo > 0.)
  {
    // ACID: type flags, root note, 2 reserved, beats, meter, tempo
    unsigned char acid[8 + acid_size]{};
    std::memcpy(acid, "acid", 4);
    put32(acid + 4, acid_size);
    put32(acid + 8, m_acid.loop ? 0x04u : 0x01u); // stretches, or one shot
    put16(acid + 12, 60);
    put16(acid + 14, 0x8000);
    put32(acid + 20, uint32_t(std::max(0., std::round(m_acid.beats))));
    put16(acid + 24, uint16_t(m_acid.denominator));
    put16(acid + 26, uint16_t(m_acid.numerator));
    const float tempo = float(m_acid.tempo);
    uint32_t bits;
    std::memcpy(&bits, &tempo, 4);
    put32(acid + 28, bits);
    ok = ok && seek(m_file, m_acidStart)
         && std::fwrite(acid, 1, sizeof(acid), m_file) == sizeof(acid);
  }

  ok = (std::fclose(m_file) == 0) && ok;
  m_file = nullptr;
  m_failed = !ok;
  return ok;
}
}
