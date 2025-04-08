// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortSerialization.hpp>

#include <score/tools/File.hpp>

#include <ossia/detail/ssize.hpp>

#include <Patternist/PatternModel.hpp>
#include <Patternist/PatternParsing.hpp>

#include <cmath>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Patternist::ProcessModel)

namespace Patternist
{
static std::vector<Patternist::Note> fromInts(std::initializer_list<int> e)
{
  std::vector<Patternist::Note> l;
  for(int v : e)
    switch(v)
    {
      case 0:
        l.push_back(Note::Rest);
        break;
      case 1:
        l.push_back(Note::Note);
        break;
      case 2:
        l.push_back(Note::Legato);
        break;
    }
  return l;
}
ProcessModel::ProcessModel(
    const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::
          ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , outlet{Process::make_midi_outlet(Id<Process::Port>(0), this)}
    , accent{Process::make_value_outlet(Id<Process::Port>(1), this)}
    , slide{Process::make_value_outlet(Id<Process::Port>(2), this)}
{
  Pattern pattern;
  pattern.length = 4;
  pattern.lanes.push_back(
      Lane{fromInts({0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0}), 38});
  pattern.lanes.push_back(
      Lane{fromInts({1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0}), 36});
  m_patterns.push_back(pattern);
  metadata().setInstanceName(*this);
  init();
}

ProcessModel::ProcessModel(
    const TimeVal& duration, const QString& customData,
    const Id<Process::ProcessModel>& id, QObject* parent)
    : Patternist::ProcessModel{duration, id, parent}
{
  if(QFile f{customData}; f.open(QIODevice::ReadOnly))
    if(auto data = score::mapAsByteArray(f); !data.isEmpty())
      if(auto pat = parsePattern(data); pat.lanes.size() > 0)
        this->m_patterns = {std::move(pat)};
}

void ProcessModel::init()
{
  m_outlets.push_back(outlet.get());
  m_outlets.push_back(accent.get());
  m_outlets.push_back(slide.get());
}

ProcessModel::~ProcessModel() { }

void ProcessModel::setChannel(int n)
{
  n = std::clamp(n, 1, 16);
  if(n != m_channel)
  {
    m_channel = n;
    channelChanged(n);
  }
}

int ProcessModel::channel() const noexcept
{
  return m_channel;
}

void ProcessModel::setCurrentPattern(int n)
{
  const int patterns = std::ssize(m_patterns);
  if(n >= patterns)
  {
    auto pattern = m_patterns[m_currentPattern];
    for(auto& lane : pattern.lanes)
      std::fill(lane.pattern.begin(), lane.pattern.end(), Note::Rest);

    while(n >= std::ssize(m_patterns))
      m_patterns.push_back(pattern);
  }

  n = std::clamp(n, 0, patterns);
  if(n != m_currentPattern)
  {
    m_currentPattern = n;
    currentPatternChanged(n);
  }
}

int ProcessModel::currentPattern() const noexcept
{
  return m_currentPattern;
}

void ProcessModel::setPattern(int n, Pattern p)
{
  m_patterns[n] = std::move(p);
  patternsChanged();
}

void ProcessModel::setPatterns(const std::vector<Pattern>& n)
{
  if(n != m_patterns)
  {
    m_patterns = n;
    patternsChanged();
  }
}

const std::vector<Pattern>& ProcessModel::patterns() const noexcept
{
  return m_patterns;
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
}

void ProcessModel::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
}

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
}
}

template <>
void DataStreamReader::read(const Patternist::Lane& proc)
{
  m_stream << proc.note << proc.pattern;
}

template <>
void DataStreamWriter::write(Patternist::Lane& proc)
{
  m_stream >> proc.note >> proc.pattern;
}

template <>
void JSONReader::read(const Patternist::Lane& proc)
{
  stream.StartObject();
  obj["Note"] = (int)proc.note;

  std::string str;
  str.reserve(proc.pattern.size());
  for(enum Patternist::Note n : proc.pattern)
  {
    switch(n)
    {
      case Patternist::Note::Rest:
        str.push_back('-');
        break;
      case Patternist::Note::Note:
        str.push_back('1');
        break;
      case Patternist::Note::Legato:
        str.push_back('2');
        break;
    }
  }

  obj["Pattern"] = str;
  stream.EndObject();
}

template <>
void JSONWriter::write(Patternist::Lane& proc)
{
  proc.note = obj["Note"].toInt();
  for(char c : obj["Pattern"].toStdString())
  {
    switch(c)
    {
      default:
      case '0':
      case '-':
      case '.':
        proc.pattern.push_back(Patternist::Note::Rest);
        break;
      case '1':
      case 'x':
      case 'X':
      case 'f':
      case 'F':
        proc.pattern.push_back(Patternist::Note::Note);
        break;
      case '2':
        proc.pattern.push_back(Patternist::Note::Legato);
        break;
    }
  }
}

template <>
void DataStreamReader::read(const Patternist::Pattern& proc)
{
  m_stream << proc.length << proc.division << proc.lanes;
}

template <>
void DataStreamWriter::write(Patternist::Pattern& proc)
{
  m_stream >> proc.length >> proc.division >> proc.lanes;
}

template <>
void JSONReader::read(const Patternist::Pattern& proc)
{
  stream.StartObject();
  obj["Length"] = proc.length;
  obj["Division"] = proc.division;
  obj["Lanes"] = proc.lanes;
  stream.EndObject();
}

template <>
void JSONWriter::write(Patternist::Pattern& proc)
{
  proc.length = obj["Length"].toInt();
  proc.division = obj["Division"].toInt();
  proc.lanes <<= obj["Lanes"];
}

template <>
void DataStreamReader::read(const Patternist::ProcessModel& proc)
{
  m_stream << *proc.outlet << *proc.accent << *proc.slide << proc.m_channel
           << proc.m_currentPattern << proc.m_patterns;

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Patternist::ProcessModel& proc)
{
  proc.outlet = Process::load_midi_outlet(*this, &proc);
  proc.accent = Process::load_value_outlet(*this, &proc);
  proc.slide = Process::load_value_outlet(*this, &proc);
  m_stream >> proc.m_channel >> *proc.accent >> *proc.slide >> proc.m_currentPattern
      >> proc.m_patterns;

  checkDelimiter();
}

template <>
void JSONReader::read(const Patternist::ProcessModel& proc)
{
  obj["Outlet"] = *proc.outlet;
  obj["Accent"] = *proc.accent;
  obj["Slide"] = *proc.slide;
  obj["Channel"] = proc.m_channel;
  obj["Pattern"] = proc.m_currentPattern;
  obj["Patterns"] = proc.m_patterns;
}

template <>
void JSONWriter::write(Patternist::ProcessModel& proc)
{
  {
    JSONWriter writer{obj["Outlet"]};
    proc.outlet = Process::load_midi_outlet(writer, &proc);
  }

  if(auto port = obj.tryGet("Accent"))
  {
    JSONWriter writer{*port};
    proc.accent = Process::load_value_outlet(writer, &proc);
  }
  else
  {
    proc.accent = Process::make_value_outlet(Id<Process::Port>(1), &proc);
  }

  if(auto port = obj.tryGet("Slide"))
  {
    JSONWriter writer{*port};
    proc.slide = Process::load_value_outlet(writer, &proc);
  }
  else
  {
    proc.slide = Process::make_value_outlet(Id<Process::Port>(2), &proc);
  }
  proc.m_channel = obj["Channel"].toInt();
  proc.m_currentPattern = obj["Pattern"].toInt();
  proc.m_patterns <<= obj["Patterns"];
}
