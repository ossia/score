#include "PatternMidiImport.hpp"

#include <ossia/detail/math.hpp>

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

#include <libremidi/reader.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace Patternist
{
MidiClip readMidiFile(const QByteArray& data) noexcept
try
{
  MidiClip clip;
  if(data.isEmpty())
    return clip;

  libremidi::reader reader;
  const auto res = reader.parse(
      reinterpret_cast<const uint8_t*>(data.constData()), std::size_t(data.size()));
  // A time division in frames per second has no quarter note to count in.
  if(res == libremidi::reader::invalid || !(reader.ticksPerBeat > 0.f)
     || !std::isfinite(reader.ticksPerBeat))
    return clip;
  const double ticks = reader.ticksPerBeat;

  int64_t end = 0;
  for(const libremidi::midi_track& track : reader.tracks)
  {
    // Index in clip.notes of the note sounding on each channel and pitch.
    std::array<std::array<int, 128>, 16> open;
    for(auto& channel : open)
      channel.fill(-1);
    const auto close = [&](int channel, int pitch, int64_t tick) {
      int& idx = open[channel][pitch];
      if(idx < 0)
        return;
      MidiNote& n = clip.notes[idx];
      n.duration = std::max(0., double(tick) / ticks - n.start);
      idx = -1;
    };

    int64_t tick = 0;
    for(const libremidi::track_event& ev : track)
    {
      tick += std::max(0, ev.tick);
      const auto& m = ev.m;
      if(m.bytes.size() < 3)
        continue;
      const auto type = m.get_message_type();
      if(type != libremidi::message_type::NOTE_ON
         && type != libremidi::message_type::NOTE_OFF)
        continue;
      const int channel = m.bytes[0] & 0x0F;
      const int pitch = m.bytes[1] & 0x7F;
      const int velocity = m.bytes[2] & 0x7F;
      // A note struck again before its release starts anew.
      close(channel, pitch, tick);
      if(type == libremidi::message_type::NOTE_ON && velocity > 0)
      {
        open[channel][pitch] = int(clip.notes.size());
        clip.notes.push_back(
            MidiNote{double(tick) / ticks, 0., uint8_t(pitch), uint8_t(velocity),
                     channel == 9});
      }
    }
    // What the track never released lasts until its end.
    for(int channel = 0; channel < 16; channel++)
      for(int pitch = 0; pitch < 128; pitch++)
        close(channel, pitch, tick);
    end = std::max(end, tick);
  }

  clip.length = double(end) / ticks;
  std::stable_sort(
      clip.notes.begin(), clip.notes.end(),
      [](const MidiNote& a, const MidiNote& b) { return a.start < b.start; });
  return clip;
}
catch(...)
{
  return {};
}

std::optional<MidiClip> midiFromMime(const QMimeData& mime)
{
  for(const char* format : {"audio/midi", "audio/x-midi"})
    if(mime.hasFormat(format))
      if(auto clip = readMidiFile(mime.data(format)); !clip.notes.empty())
        return clip;

  for(const QUrl& url : mime.urls())
  {
    const QString path = url.toLocalFile();
    const QString suffix = QFileInfo{path}.suffix().toLower();
    if(suffix != QLatin1String("mid") && suffix != QLatin1String("midi")
       && suffix != QLatin1String("smf") && suffix != QLatin1String("gm"))
      continue;
    QFile f{path};
    if(!f.open(QIODevice::ReadOnly))
      continue;
    if(auto clip = readMidiFile(f.readAll()); !clip.notes.empty())
      return clip;
  }
  return std::nullopt;
}

namespace
{
struct Hit
{
  //! Steps from the grid, -0.5 - 0.5.
  double offset{};
  //! In steps.
  double duration{};
  uint8_t velocity{};
  bool drum{};
};

//! Under this much of a step, a note is taken to be on the grid: what a
//! sequencer exports stays free of offsets.
constexpr double on_grid = 0.05;
//! From this much of a step on, a note fills it.
constexpr double full_gate = 0.75;
}

MidiMatch matchToGrid(const Pattern& current, const MidiClip& clip)
{
  MidiMatch res;
  Pattern& p = res.pattern;
  p = current;
  normalize(p);
  if(clip.notes.empty())
    return res;

  // The notes of each pitch, high pitches first: the order new lanes take.
  std::map<uint8_t, std::vector<const MidiNote*>, std::greater<>> by_pitch;
  for(const MidiNote& n : clip.notes)
    if(std::isfinite(n.start) && n.start >= 0. && n.pitch <= 127)
      by_pitch[n.pitch].push_back(&n);

  // The clip replaces what the note lanes played. Their length and rate stay:
  // they are the grid the clip is put on.
  for(Lane& l : p.lanes)
  {
    if(isControlLane(l))
      continue;
    std::fill(l.pattern.begin(), l.pattern.end(), Note::Rest);
    std::fill(l.params.begin(), l.params.end(), StepParams{});
  }

  for(const auto& [pitch, notes] : by_pitch)
  {
    auto it = std::find_if(p.lanes.begin(), p.lanes.end(), [pitch = pitch](const Lane& l) {
      return l.note == pitch;
    });
    if(it == p.lanes.end())
    {
      // A new lane follows the pattern's grid: none for a pitch that only
      // plays past its end.
      const double new_steps_per_quarter = p.division / 4.;
      const bool fits = std::any_of(notes.begin(), notes.end(), [&](const MidiNote* n) {
        return n->start * new_steps_per_quarter < p.length;
      });
      if(!fits || std::ssize(p.lanes) >= Pattern::maxLanes)
      {
        res.dropped += int(notes.size());
        continue;
      }
      // Above the first note lane that plays lower, or under the last one.
      auto at = std::find_if(p.lanes.begin(), p.lanes.end(), [pitch = pitch](const Lane& l) {
        return !isControlLane(l) && l.note < pitch;
      });
      if(at == p.lanes.end())
        while(at != p.lanes.begin() && isControlLane(*(at - 1)))
          --at;
      Lane lane;
      lane.note = pitch;
      lane.pattern.assign(p.length, Note::Rest);
      lane.params.resize(p.length);
      it = p.lanes.insert(at, std::move(lane));
    }

    Lane& l = *it;
    const int length = laneLength(p, l);
    const double steps_per_quarter = laneDivision(p, l) / 4.;

    std::map<int, Hit> hits;
    for(const MidiNote* n : notes)
    {
      const double x = n->start * steps_per_quarter;
      if(!std::isfinite(x) || x >= length)
      {
        res.dropped++;
        continue;
      }
      int step = int(std::floor(x + 0.5));
      double offset = x - step;
      // Just before the end: it leads into the first step of the next pass.
      if(step >= length)
        step = 0;
      if(std::abs(offset) < on_grid)
        offset = 0.;
      const Hit hit{
          offset, std::isfinite(n->duration) ? n->duration * steps_per_quarter : 0.,
          std::clamp<uint8_t>(n->velocity, 1, 127), n->drum};
      auto [hit_it, inserted] = hits.try_emplace(step, hit);
      if(!inserted)
      {
        res.merged++;
        Hit& kept = hit_it->second;
        const double a = std::abs(hit.offset), b = std::abs(kept.offset);
        if(a < b || (a == b && hit.velocity > kept.velocity))
          kept = hit;
      }
    }
    if(hits.empty())
      continue;

    uint8_t loudest = 1;
    for(const auto& [step, hit] : hits)
      loudest = std::max(loudest, hit.velocity);
    l.velocity = loudest / 127.f;

    for(const auto& [step, hit] : hits)
    {
      l.pattern[step] = Note::Note;
      StepParams& sp = l.params[step];
      sp.velocity = float(hit.velocity) / loudest;
      sp.offset = float(std::clamp(hit.offset, -0.5, 0.5));
      res.notes++;
      if(hit.drum)
        continue;
      if(hit.duration < full_gate)
        sp.gate = float(std::max(0.05, hit.duration));
      // Tied over the steps it covers, up to the next note of the lane.
      const int last = std::min(step + int(std::lround(hit.duration)), length) - 1;
      for(int s = step + 1; s <= last && !hits.contains(s); s++)
        l.pattern[s] = Note::Legato;
    }
  }
  normalize(p);
  return res;
}
}
