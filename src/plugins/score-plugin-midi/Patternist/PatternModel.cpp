// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Patternist/PatternModel.hpp>
#include <Process/Dataflow/Port.hpp>
#include <cmath>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Patternist::ProcessModel)

namespace Patternist
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration,
                            id,
                            Metadata<ObjectKey_k, ProcessModel>::get(),
                            parent}
    , outlet{Process::make_midi_outlet(Id<Process::Port>(0), this)}
{
  Pattern pattern;
  pattern.length = 4;
  pattern.lanes.push_back(Lane{
                            {1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0},
                            64
                          });
  pattern.lanes.push_back(Lane{
                            {0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0},
                            32
                          });
  m_patterns.push_back(pattern);
  metadata().setInstanceName(*this);
  init();
}

void ProcessModel::init() { m_outlets.push_back(outlet.get()); }

ProcessModel::~ProcessModel() {}

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
  n = std::clamp(n, 0, (int)m_patterns.size());
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
  m_patterns[n] = p;
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
  obj["Note"] = (int)proc.note;

  std::string str;
  str.reserve(proc.pattern.size());
  for(bool b: proc.pattern)
    str.push_back(b? 'X' : '.');

  obj["Pattern"] = str;
}

template <>
void JSONWriter::write(Patternist::Lane& proc)
{
  proc.note = obj["Note"].toInt();
  for(char c: obj["Pattern"].toStdString())
    proc.pattern.push_back(c == 'X');
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
  obj["Length"] = proc.length;
  obj["Division"] = proc.division;
  obj["Lanes"] = proc.lanes;
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
  m_stream << *proc.outlet
           << proc.m_channel << proc.m_currentPattern << proc.m_patterns;

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Patternist::ProcessModel& proc)
{
  proc.outlet = Process::load_midi_outlet(*this, &proc);
  m_stream >> proc.m_channel >> proc.m_currentPattern >> proc.m_patterns;

  checkDelimiter();
}

template <>
void JSONReader::read(const Patternist::ProcessModel& proc)
{
  obj["Outlet"] = *proc.outlet;
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
  proc.m_channel = obj["Channel"].toInt();
  proc.m_currentPattern = obj["Pattern"].toInt();
  proc.m_patterns <<= obj["Patterns"];
}
