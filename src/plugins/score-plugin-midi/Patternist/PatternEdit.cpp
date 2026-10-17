#include "PatternEdit.hpp"

#include <score/serialization/JSONVisitor.hpp>

#include <ossia/detail/math.hpp>

#include <QCoreApplication>
#include <QMimeData>

#include <algorithm>

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
  const int n = p.length;
  const int by = ((steps % n) + n) % n;
  for(Lane& lane : p.lanes)
    std::rotate(
        lane.pattern.begin(), lane.pattern.begin() + (n - by) % n,
        lane.pattern.begin() + n);
}

void randomize(Pattern& p, std::mt19937& rng) noexcept
{
  normalize(p);
  // Steps falling on a quarter note are the beats.
  const int stepsPerBeat = std::max(1, p.division / 4);
  std::uniform_real_distribution<double> dist{0., 1.};
  for(Lane& lane : p.lanes)
  {
    for(int i = 0; i < p.length; i++)
    {
      const double chance = (i % stepsPerBeat == 0) ? 0.5 : 0.2;
      lane.pattern[i] = dist(rng) < chance ? Note::Note : Note::Rest;
    }
  }
}

void addLane(Pattern& p)
{
  if(p.lanes.empty())
    p.lanes.push_back(Lane{std::vector<Note>(p.length, Note::Rest), 36});
  else
    p.lanes.push_back(p.lanes.back());
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
    json = mime.text().toUtf8();
  if(json.isEmpty())
    return std::nullopt;

  const rapidjson::Document doc = readJson(json);
  if(!doc.IsObject() || !doc.HasMember("Lanes") || !doc["Lanes"].IsArray()
     || !doc.HasMember("Length") || !doc.HasMember("Division"))
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
}
