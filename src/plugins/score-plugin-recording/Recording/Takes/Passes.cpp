#include "Passes.hpp"

#include <dr_wav.h>

#include <algorithm>

namespace Recording
{
std::vector<Pass> passesOf(int64_t start, int64_t end, int64_t length)
{
  std::vector<Pass> r;
  if(end <= start || length <= 0)
    return r;
  for(int64_t s = start; s + length <= end; s += length)
    r.push_back({s, s + length});
  if(r.empty())
    r.push_back({start, end});
  return r;
}

bool writeWavSegment(
    const QString& source, const QString& dest, int64_t from, int64_t count,
    int64_t rotate, WavStreamWriter::Format format)
{
  if(count <= 0)
    return false;
  drwav wav;
  if(!drwav_init_file(&wav, source.toStdString().c_str(), nullptr))
    return false;
  const int channels = int(wav.channels);
  const int64_t total = int64_t(wav.totalPCMFrameCount);

  // The segment, one channel after the other
  std::vector<float> interleaved(std::size_t(count) * channels, 0.f);
  if(from < total)
  {
    drwav_seek_to_pcm_frame(&wav, drwav_uint64(std::max<int64_t>(0, from)));
    const int64_t n = std::min(count, total - from);
    drwav_read_pcm_frames_f32(&wav, drwav_uint64(n), interleaved.data());
  }
  const int rate = int(wav.sampleRate);
  drwav_uninit(&wav);

  rotate = ((rotate % count) + count) % count;
  std::vector<std::vector<float>> planar(channels, std::vector<float>(count));
  for(int64_t i = 0; i < count; i++)
  {
    // Frame i of the result is frame i - rotate of the segment, round it
    const int64_t src = (i - rotate + count) % count;
    for(int c = 0; c < channels; c++)
      planar[c][i] = interleaved[std::size_t(src) * channels + c];
  }

  WavStreamWriter w;
  if(!w.open(dest.toStdString(), channels, rate, format))
    return false;
  std::vector<const float*> ptrs;
  for(auto& ch : planar)
    ptrs.push_back(ch.data());
  w.write(ptrs.data(), count);
  return w.close();
}

std::vector<RecordedNote>
notesOfPass(const std::vector<RecordedNote>& notes, Pass p, int64_t rotate)
{
  std::vector<RecordedNote> r;
  const int64_t length = p.end - p.start;
  if(length <= 0)
    return r;
  rotate = ((rotate % length) + length) % length;
  for(auto n : notes)
  {
    if(n.start < p.start || n.start >= p.end)
      continue;
    int64_t s = n.start - p.start + rotate;
    if(s >= length)
      s -= length;
    // Cut at the end of the pass, then at the end of the turned loop
    int64_t d = std::min(n.duration, p.end - n.start);
    d = std::min(d, length - s);
    if(d <= 0)
      continue;
    n.start = s;
    n.duration = d;
    r.push_back(n);
  }
  std::stable_sort(r.begin(), r.end(), [](auto& a, auto& b) { return a.start < b.start; });
  return r;
}

std::vector<ValueTake::Event>
valuesOfPass(const std::vector<ValueTake::Event>& events, Pass p, int64_t rotate)
{
  std::vector<ValueTake::Event> r;
  const int64_t length = p.end - p.start;
  if(length <= 0)
    return r;
  rotate = ((rotate % length) + length) % length;
  for(const auto& e : events)
  {
    if(e.date < p.start || e.date >= p.end)
      continue;
    int64_t d = e.date - p.start + rotate;
    if(d >= length)
      d -= length;
    r.push_back({d, e.value});
  }
  std::stable_sort(r.begin(), r.end(), [](auto& a, auto& b) { return a.date < b.date; });
  return r;
}
}
