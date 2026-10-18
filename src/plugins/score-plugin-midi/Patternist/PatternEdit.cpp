#include "PatternEdit.hpp"

#include <score/serialization/JSONVisitor.hpp>

#include <ossia/detail/math.hpp>

#include <QCoreApplication>
#include <QMimeData>

#include <algorithm>
#include <cmath>

namespace Patternist
{
static constexpr auto patternMimeType = "application/x-score-patternist-pattern";

Note toggledNote(Note current) noexcept
{
  return current == Note::Rest ? Note::Note : Note::Rest;
}

bool setStep(Pattern& p, int lane, int step, Note n) noexcept
{
  if(!ossia::valid_index(lane, p.lanes))
    return false;
  auto& steps = p.lanes[lane].pattern;
  if(!ossia::valid_index(step, steps) || steps[step] == n)
    return false;
  steps[step] = n;
  return true;
}

void clear(Pattern& p) noexcept
{
  for(Lane& lane : p.lanes)
    std::fill(lane.pattern.begin(), lane.pattern.end(), Note::Rest);
}

void rotate(Pattern& p, int steps) noexcept
{
  normalize(p);
  for(Lane& lane : p.lanes)
  {
    const int len = std::min<int>(laneLength(p, lane), lane.pattern.size());
    if(len <= 0)
      continue;
    const int r = ((steps % len) + len) % len;
    std::rotate(
        lane.pattern.begin(), lane.pattern.begin() + (len - r) % len,
        lane.pattern.begin() + len);
    std::rotate(
        lane.params.begin(), lane.params.begin() + (len - r) % len,
        lane.params.begin() + len);
  }
}

void randomize(Pattern& p, std::mt19937& rng, int lane) noexcept
{
  normalize(p);
  std::uniform_real_distribution<double> dist{0., 1.};
  for(int li = 0; li < int(p.lanes.size()); li++)
  {
    if(lane >= 0 && li != lane)
      continue;
    Lane& l = p.lanes[li];
    // Steps falling on a quarter note are the beats.
    const int stepsPerBeat = std::max(1, laneDivision(p, l) / 4);
    const int len = laneLength(p, l);
    for(int i = 0; i < len && i < int(l.pattern.size()); i++)
    {
      const double chance = (i % stepsPerBeat == 0) ? 0.5 : 0.2;
      l.pattern[i] = dist(rng) < chance ? Note::Note : Note::Rest;
    }
  }
}

void euclidean(const Pattern& p, Lane& l, int hits, int rotation, Combine c) noexcept
{
  const int len = std::min<int>(laneLength(p, l), l.pattern.size());
  for(int i = 0; i < len; i++)
  {
    const bool hit = euclidHit(i, len, hits, rotation);
    Note& n = l.pattern[i];
    switch(c)
    {
      case Combine::Replace:
        n = hit ? Note::Note : Note::Rest;
        break;
      case Combine::Add:
        if(hit && n == Note::Rest)
          n = Note::Note;
        break;
      case Combine::Mask:
        if(!hit)
          n = Note::Rest;
        break;
    }
  }
}

bool transpose(Pattern& p, int semitones) noexcept
{
  bool moved = false;
  for(Lane& l : p.lanes)
  {
    if(l.note > 127)
      continue;
    const int n = std::clamp(int(l.note) + semitones, 0, 127);
    moved |= n != l.note;
    l.note = uint8_t(n);
  }
  return moved;
}

void addLane(Pattern& p)
{
  if(std::ssize(p.lanes) >= Pattern::maxLanes)
    return;
  auto last = std::find_if(
      p.lanes.rbegin(), p.lanes.rend(), [](const Lane& l) { return l.note < 128; });
  Lane lane = last != p.lanes.rend()
                  ? *last
                  : Lane{std::vector<Note>(p.length, Note::Rest), 36};
  auto controls = std::find_if(
      p.lanes.begin(), p.lanes.end(), [](const Lane& l) { return isControlLane(l); });
  if(last != p.lanes.rend() && controls < last.base())
    controls = last.base();
  p.lanes.insert(controls, std::move(lane));
}

bool hasLane(const Pattern& p, uint8_t note) noexcept
{
  return std::any_of(
      p.lanes.begin(), p.lanes.end(), [note](const Lane& l) { return l.note == note; });
}

bool addControlLane(Pattern& p, uint8_t note) noexcept
{
  if(std::ssize(p.lanes) >= Pattern::maxLanes || hasLane(p, note))
    return false;
  Lane lane{std::vector<Note>(p.length, Note::Rest), note};
  // Starts as a moderate accent, and its depth sets how far each step goes.
  lane.velocity = 0.5f;
  lane.params.resize(lane.pattern.size());
  p.lanes.push_back(std::move(lane));
  return true;
}

void removeLane(Pattern& p, int lane)
{
  if(p.lanes.size() > 1 && ossia::valid_index(lane, p.lanes))
    p.lanes.erase(p.lanes.begin() + lane);
}

void setLength(Pattern& p, int length)
{
  p.length = length;
  normalize(p);
}

QMimeData* patternToMime(const Pattern& p)
{
  auto mime = new QMimeData;
  const QByteArray json = toJson(p);
  mime->setData(patternMimeType, json);
  mime->setText(QString::fromUtf8(json));
  return mime;
}

std::optional<Pattern> patternFromMime(const QMimeData& mime)
{
  QByteArray json = mime.data(patternMimeType);
  if(json.isEmpty())
    json = mime.text().trimmed().toUtf8();
  // Most of what goes through the clipboard is not JSON at all: parse quietly.
  if(!json.startsWith('{'))
    return std::nullopt;

  rapidjson::Document doc;
  doc.Parse(json.constData(), json.size());
  if(doc.HasParseError() || !doc.IsObject() || !doc.HasMember("Lanes")
     || !doc["Lanes"].IsArray() || !doc.HasMember("Length")
     || !doc.HasMember("Division"))
    return std::nullopt;

  JSONWriter wr{doc};
  Pattern p;
  wr.writeTo(p);
  if(p.lanes.empty())
    return std::nullopt;
  return p;
}

Pattern copySteps(const Pattern& p, const StepSelection& s)
{
  Pattern res;
  res.division = p.division;
  res.length = 1;
  for(int r = 0; r < int(s.steps.size()); r++)
  {
    Lane l;
    if(ossia::valid_index(s.lane0 + r, p.lanes))
    {
      const Lane& src = p.lanes[s.lane0 + r];
      for(int step : s.steps[r])
      {
        l.pattern.push_back(writtenNote(p, src, step));
        l.params.push_back(stepParams(src, step));
      }
    }
    l.length = std::clamp<int>(l.pattern.size(), 0, Pattern::maxLength);
    res.length = std::max(res.length, l.length);
    res.lanes.push_back(std::move(l));
  }
  return res;
}

void pasteSteps(Pattern& p, const Pattern& steps, int lane, const std::vector<int>& starts)
{
  normalize(p);
  if(lane < 0)
    return;
  for(int r = 0; r < int(steps.lanes.size()) && r < int(starts.size()); r++)
  {
    if(lane + r >= int(p.lanes.size()))
      break;
    Lane& dst = p.lanes[lane + r];
    const Lane& src = steps.lanes[r];
    const int len = std::min<int>(laneLength(p, dst), dst.pattern.size());
    // A copied lane is padded up to the longest one: its length says how
    // many steps it holds.
    const int count
        = std::min({laneLength(steps, src), int(src.pattern.size()), len});
    if(len <= 0 || starts[r] < 0)
      continue;
    for(int i = 0; i < count; i++)
    {
      const int step = (starts[r] + i) % len;
      dst.pattern[step] = src.pattern[i];
      dst.params[step] = stepParams(src, i);
    }
  }
}

void clearSteps(Pattern& p, const StepSelection& s) noexcept
{
  normalize(p);
  for(int r = 0; r < int(s.steps.size()); r++)
  {
    if(!ossia::valid_index(s.lane0 + r, p.lanes))
      break;
    Lane& l = p.lanes[s.lane0 + r];
    for(int step : s.steps[r])
    {
      if(ossia::valid_index(step, l.pattern))
      {
        l.pattern[step] = Note::Rest;
        l.params[step] = StepParams{};
      }
    }
  }
}

static constexpr auto stepsMimeType = "application/x-score-patternist-steps";

QMimeData* stepsToMime(const Pattern& steps)
{
  auto mime = new QMimeData;
  mime->setData(stepsMimeType, toJson(steps));
  return mime;
}

std::optional<Pattern> stepsFromMime(const QMimeData& mime)
{
  const QByteArray json = mime.data(stepsMimeType);
  if(!json.startsWith('{'))
    return std::nullopt;
  rapidjson::Document doc;
  doc.Parse(json.constData(), json.size());
  if(doc.HasParseError() || !doc.IsObject() || !doc.HasMember("Lanes"))
    return std::nullopt;
  JSONWriter wr{doc};
  Pattern p;
  wr.writeTo(p);
  if(p.lanes.empty())
    return std::nullopt;
  return p;
}

//! General MIDI percussion, notes 35 to 81.
static QString drumName(int note)
{
  static const char* const names[] = {
      QT_TRANSLATE_NOOP("Patternist", "Kick 2"),
      QT_TRANSLATE_NOOP("Patternist", "Kick"),
      QT_TRANSLATE_NOOP("Patternist", "Stick"),
      QT_TRANSLATE_NOOP("Patternist", "Snare"),
      QT_TRANSLATE_NOOP("Patternist", "Clap"),
      QT_TRANSLATE_NOOP("Patternist", "Snare 2"),
      QT_TRANSLATE_NOOP("Patternist", "Low tom 2"),
      QT_TRANSLATE_NOOP("Patternist", "Closed HH"),
      QT_TRANSLATE_NOOP("Patternist", "Low tom"),
      QT_TRANSLATE_NOOP("Patternist", "Pedal HH"),
      QT_TRANSLATE_NOOP("Patternist", "Mid tom 2"),
      QT_TRANSLATE_NOOP("Patternist", "Open HH"),
      QT_TRANSLATE_NOOP("Patternist", "Mid tom"),
      QT_TRANSLATE_NOOP("Patternist", "High tom 2"),
      QT_TRANSLATE_NOOP("Patternist", "Crash"),
      QT_TRANSLATE_NOOP("Patternist", "High tom"),
      QT_TRANSLATE_NOOP("Patternist", "Ride"),
      QT_TRANSLATE_NOOP("Patternist", "China"),
      QT_TRANSLATE_NOOP("Patternist", "Ride bell"),
      QT_TRANSLATE_NOOP("Patternist", "Tambourine"),
      QT_TRANSLATE_NOOP("Patternist", "Splash"),
      QT_TRANSLATE_NOOP("Patternist", "Cowbell"),
      QT_TRANSLATE_NOOP("Patternist", "Crash 2"),
      QT_TRANSLATE_NOOP("Patternist", "Vibraslap"),
      QT_TRANSLATE_NOOP("Patternist", "Ride 2"),
      QT_TRANSLATE_NOOP("Patternist", "Hi bongo"),
      QT_TRANSLATE_NOOP("Patternist", "Lo bongo"),
      QT_TRANSLATE_NOOP("Patternist", "Mute conga"),
      QT_TRANSLATE_NOOP("Patternist", "Hi conga"),
      QT_TRANSLATE_NOOP("Patternist", "Lo conga"),
      QT_TRANSLATE_NOOP("Patternist", "Hi timbale"),
      QT_TRANSLATE_NOOP("Patternist", "Lo timbale"),
      QT_TRANSLATE_NOOP("Patternist", "Hi agogo"),
      QT_TRANSLATE_NOOP("Patternist", "Lo agogo"),
      QT_TRANSLATE_NOOP("Patternist", "Cabasa"),
      QT_TRANSLATE_NOOP("Patternist", "Maracas"),
      QT_TRANSLATE_NOOP("Patternist", "Whistle"),
      QT_TRANSLATE_NOOP("Patternist", "Whistle 2"),
      QT_TRANSLATE_NOOP("Patternist", "Guiro"),
      QT_TRANSLATE_NOOP("Patternist", "Guiro 2"),
      QT_TRANSLATE_NOOP("Patternist", "Claves"),
      QT_TRANSLATE_NOOP("Patternist", "Hi block"),
      QT_TRANSLATE_NOOP("Patternist", "Lo block"),
      QT_TRANSLATE_NOOP("Patternist", "Cuica"),
      QT_TRANSLATE_NOOP("Patternist", "Cuica 2"),
      QT_TRANSLATE_NOOP("Patternist", "Triangle"),
      QT_TRANSLATE_NOOP("Patternist", "Triangle 2"),
  };
  constexpr int first = 35;
  if(note < first || note >= first + int(std::size(names)))
    return {};
  return QCoreApplication::translate("Patternist", names[note - first]);
}

static QString noteName(int note)
{
  static constexpr const char* names[12]
      = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  return QStringLiteral("%1%2").arg(names[note % 12]).arg(note / 12 - 1);
}

QString laneLabel(int note, LaneLabels mode)
{
  if(note == 255 || note == -1)
    return QObject::tr("Accent");
  if(note == 254 || note == -2)
    return QObject::tr("Slide");
  if(note < 0 || note > 127)
    return {};

  switch(mode)
  {
    case LaneLabels::Name:
      return noteName(note);
    case LaneLabels::Number:
      return QString::number(note);
    case LaneLabels::DrumName:
      if(auto name = drumName(note); !name.isEmpty())
        return name;
      return noteName(note);
    case LaneLabels::NameAndNumber:
    default:
      return QStringLiteral("%1 %2").arg(noteName(note)).arg(note);
  }
}

FieldRange fieldRange(StepField f) noexcept
{
  switch(f)
  {
    case StepField::Velocity:
    case StepField::VelocityRandom:
    case StepField::Chance:
    case StepField::Gate:
    case StepField::GateRandom:
      return {0.f, 1.f, 0.f};
    case StepField::Offset:
      return {-0.5f, 0.5f, 0.f};
    case StepField::OffsetRandom:
      return {0.f, 0.5f, 0.f};
    case StepField::Pace:
      return {-1.f, 1.f, 0.f};
    case StepField::Ratchet:
      return {1.f, 8.f, 1.f};
    case StepField::Pitch:
      return {-24.f, 24.f, 0.f};
    case StepField::PitchRandom:
      return {0.f, 24.f, 0.f};
  }
  return {0.f, 1.f, 0.f};
}

QString fieldName(StepField f)
{
  switch(f)
  {
    case StepField::Velocity:
      return QObject::tr("Velocity");
    case StepField::VelocityRandom:
      return QObject::tr("Velocity random");
    case StepField::Chance:
      return QObject::tr("Chance");
    case StepField::Offset:
      return QObject::tr("Timing");
    case StepField::OffsetRandom:
      return QObject::tr("Timing random");
    case StepField::Gate:
      return QObject::tr("Gate");
    case StepField::GateRandom:
      return QObject::tr("Gate random");
    case StepField::Ratchet:
      return QObject::tr("Ratchet");
    case StepField::Pace:
      return QObject::tr("Pace");
    case StepField::Pitch:
      return QObject::tr("Pitch");
    case StepField::PitchRandom:
      return QObject::tr("Pitch random");
  }
  return {};
}

float stepField(const StepParams& p, StepField f) noexcept
{
  switch(f)
  {
    case StepField::Velocity:
      return p.velocity;
    case StepField::VelocityRandom:
      return p.velocityRandom;
    case StepField::Chance:
      return p.chance;
    case StepField::Offset:
      return p.offset;
    case StepField::OffsetRandom:
      return p.offsetRandom;
    case StepField::Gate:
      return p.gate;
    case StepField::GateRandom:
      return p.gateRandom;
    case StepField::Ratchet:
      return p.ratchet;
    case StepField::Pace:
      return p.pace;
    case StepField::Pitch:
      return p.pitch;
    case StepField::PitchRandom:
      return p.pitchRandom;
  }
  return 0.f;
}

void setStepField(StepParams& p, StepField f, float v) noexcept
{
  const auto r = fieldRange(f);
  v = std::clamp(v, r.min, r.max);
  switch(f)
  {
    case StepField::Velocity:
      p.velocity = v;
      break;
    case StepField::VelocityRandom:
      p.velocityRandom = v;
      break;
    case StepField::Chance:
      p.chance = v;
      break;
    case StepField::Offset:
      p.offset = v;
      break;
    case StepField::OffsetRandom:
      p.offsetRandom = v;
      break;
    case StepField::Gate:
      p.gate = v;
      break;
    case StepField::GateRandom:
      p.gateRandom = v;
      break;
    case StepField::Ratchet:
      p.ratchet = uint8_t(std::lround(v));
      break;
    case StepField::Pace:
      p.pace = v;
      break;
    case StepField::Pitch:
      p.pitch = int8_t(std::lround(v));
      break;
    case StepField::PitchRandom:
      p.pitchRandom = uint8_t(std::lround(v));
      break;
  }
}

QString fieldText(StepField f, float v)
{
  switch(f)
  {
    case StepField::Velocity:
    case StepField::Chance:
    case StepField::Gate:
      return QStringLiteral("%1%").arg(int(std::lround(v * 100.f)));
    case StepField::Offset:
      return QStringLiteral("%1%2%")
          .arg(v > 0 ? "+" : "")
          .arg(int(std::lround(v * 100.f)));
    case StepField::Ratchet:
      return QStringLiteral("x%1").arg(int(std::lround(v)));
    case StepField::Pace:
      return QStringLiteral("%1%2%")
          .arg(v > 0 ? "+" : "")
          .arg(int(std::lround(v * 100.f)));
    case StepField::Pitch:
      return QStringLiteral("%1%2").arg(v > 0 ? "+" : "").arg(int(std::lround(v)));
    case StepField::VelocityRandom:
    case StepField::OffsetRandom:
    case StepField::GateRandom:
      return QStringLiteral("±%1%").arg(int(std::lround(v * 100.f)));
    case StepField::PitchRandom:
      return QStringLiteral("±%1").arg(int(std::lround(v)));
  }
  return {};
}

float randomFieldValue(StepField f, std::mt19937& rng) noexcept
{
  std::uniform_real_distribution<float> unit{0.f, 1.f};
  switch(f)
  {
    case StepField::Velocity:
      return 0.4f + 0.6f * unit(rng);
    case StepField::VelocityRandom:
      // A light humanization on most steps.
      return unit(rng) < 0.3f ? 0.f : 0.05f + 0.2f * unit(rng);
    case StepField::Chance:
      // Most steps always play.
      return unit(rng) < 0.5f ? 1.f : 0.25f + 0.75f * unit(rng);
    case StepField::Offset:
      return std::normal_distribution<float>{0.f, 0.06f}(rng);
    case StepField::OffsetRandom:
      // A slight looseness on some steps.
      return unit(rng) < 0.6f ? 0.f : 0.02f + 0.08f * unit(rng);
    case StepField::Gate:
      return 0.2f + 0.8f * unit(rng);
    case StepField::GateRandom:
      // Some steps a little looser.
      return unit(rng) < 0.6f ? 0.f : 0.05f + 0.25f * unit(rng);
    case StepField::Ratchet: {
      const float r = unit(rng);
      return r < 0.75f ? 1.f : r < 0.9f ? 2.f : r < 0.97f ? 3.f : 4.f;
    }
    case StepField::Pitch: {
      // Octaves and fifths, around the lane's note.
      static constexpr int intervals[] = {0, 0, 0, 0, 12, -12, 7, -5, 5, 3};
      return intervals[std::uniform_int_distribution<int>{0, 9}(rng)];
    }
    case StepField::Pace:
      // Most rolls stay even.
      return unit(rng) < 0.6f ? 0.f : -0.8f + 1.6f * unit(rng);
    case StepField::PitchRandom:
      // Few steps wander, and not far.
      return unit(rng) < 0.7f ? 0.f : float(std::uniform_int_distribution<int>{1, 3}(rng));
  }
  return 0.f;
}

float anyFieldValue(StepField f, std::mt19937& rng) noexcept
{
  const auto r = fieldRange(f);
  switch(f)
  {
    case StepField::Ratchet:
    case StepField::Pitch:
    case StepField::PitchRandom:
      return float(std::uniform_int_distribution<int>{int(r.min), int(r.max)}(rng));
    default:
      return std::uniform_real_distribution<float>{r.min, r.max}(rng);
  }
}

void randomizeField(Pattern& p, std::mt19937& rng, StepField f, int lane) noexcept
{
  normalize(p);
  for(int li = 0; li < int(p.lanes.size()); li++)
  {
    if(lane >= 0 && li != lane)
      continue;
    Lane& l = p.lanes[li];
    const int len = std::min<int>(laneLength(p, l), l.params.size());
    for(int i = 0; i < len; i++)
      setStepField(l.params[i], f, randomFieldValue(f, rng));
  }
  normalize(p);
}

void resetField(Pattern& p, StepField f, int lane) noexcept
{
  normalize(p);
  const float v = stepField(StepParams{}, f);
  for(int li = 0; li < int(p.lanes.size()); li++)
  {
    if(lane >= 0 && li != lane)
      continue;
    for(StepParams& s : p.lanes[li].params)
      setStepField(s, f, v);
  }
}
}
