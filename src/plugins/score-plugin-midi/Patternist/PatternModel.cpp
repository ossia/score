// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortSerialization.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/tools/File.hpp>

#include <ossia/detail/ssize.hpp>

#include <Patternist/PatternModel.hpp>
#include <Patternist/PatternParsing.hpp>

#include <cmath>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Patternist::ProcessModel)

namespace Patternist
{
//! Rates, in the convention ossia::token_request::get_quantification_dates
//! consumes: above 1 subdivides the whole note, 1 and below counts bars, and
//! 0 does not quantize at all.
static std::vector<std::pair<QString, ossia::value>> quantificationChoices()
{
  return {{QObject::tr("Free"), 0.},     {QObject::tr("8 bars"), 0.125},
          {QObject::tr("4 bars"), 0.25}, {QObject::tr("2 bars"), 0.5},
          {QObject::tr("1 bar"), 1.},    {QObject::tr("1/2"), 2.},
          {QObject::tr("1/4"), 4.},      {QObject::tr("1/8"), 8.},
          {QObject::tr("1/16"), 16.},    {QObject::tr("1/32"), 32.}};
}

//! The pattern list grows on demand, so the port is a plain index rather than
//! a list of the patterns that happen to exist right now.
static std::unique_ptr<Process::ControlInlet> makePatternSelect(QObject* parent)
{
  return std::make_unique<Process::IntSpinBox>(
      0, 127, 0, QObject::tr("Pattern"), Id<Process::Port>(0), parent);
}

static std::unique_ptr<Process::ControlInlet> makeSwitchQuantification(QObject* parent)
{
  return std::make_unique<Process::ComboBox>(
      quantificationChoices(), 1., QObject::tr("Quantization"), Id<Process::Port>(1),
      parent);
}

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
    , patternSelect{makePatternSelect(this)}
    , switchQuantification{makeSwitchQuantification(this)}
    , outlet{std::make_unique<Process::MidiOutlet>(
          "MIDI Out", Id<Process::Port>(0), this)}
    , accent{std::make_unique<Process::ValueOutlet>(
          "Accent", Id<Process::Port>(1), this)}
    , slide{std::make_unique<Process::ValueOutlet>("Slide", Id<Process::Port>(2), this)}
{
  Pattern pattern;
  pattern.length = 16;
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
      if(auto pat = parsePatterns(data); pat.size() > 0)
        this->m_patterns = std::move(pat);
}

void ProcessModel::init()
{
  // The pattern grid is what the layer draws; nothing there would draw the
  // controls, so they stay visible as ordinary ports.
  patternSelect->displayHandledExplicitly = false;
  switchQuantification->displayHandledExplicitly = false;

  m_inlets.push_back(patternSelect.get());
  m_inlets.push_back(switchQuantification.get());
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
  n = std::max(n, 0);
  if(m_patterns.empty())
    return;

  if(n >= std::ssize(m_patterns))
  {
    auto pattern
        = m_patterns[std::clamp(m_currentPattern, 0, int(m_patterns.size()) - 1)];
    for(auto& lane : pattern.lanes)
      std::fill(lane.pattern.begin(), lane.pattern.end(), Note::Rest);

    while(n >= std::ssize(m_patterns))
      m_patterns.push_back(pattern);

    patternsChanged();
  }

  n = std::clamp(n, 0, int(std::ssize(m_patterns)) - 1);
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
  m_stream << *proc.patternSelect << *proc.switchQuantification << *proc.outlet
           << *proc.accent << *proc.slide << proc.m_channel << proc.m_currentPattern
           << proc.m_patterns;

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Patternist::ProcessModel& proc)
{
  proc.patternSelect = Process::load_control_inlet(*this, &proc);
  proc.switchQuantification = Process::load_control_inlet(*this, &proc);
  proc.outlet = Process::load_midi_outlet(*this, &proc);
  proc.accent = Process::load_value_outlet(*this, &proc);
  proc.slide = Process::load_value_outlet(*this, &proc);
  m_stream >> proc.m_channel >> proc.m_currentPattern >> proc.m_patterns;

  checkDelimiter();
}

template <>
void JSONReader::read(const Patternist::ProcessModel& proc)
{
  obj["PatternSelect"] = *proc.patternSelect;
  obj["SwitchQuantification"] = *proc.switchQuantification;
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
  if(auto port = obj.tryGet("PatternSelect"))
  {
    JSONWriter writer{*port};
    proc.patternSelect = Process::load_control_inlet(writer, &proc);
  }
  else
  {
    proc.patternSelect = Patternist::makePatternSelect(&proc);
  }

  if(auto port = obj.tryGet("SwitchQuantification"))
  {
    JSONWriter writer{*port};
    proc.switchQuantification = Process::load_control_inlet(writer, &proc);
  }
  else
  {
    proc.switchQuantification = Patternist::makeSwitchQuantification(&proc);
  }

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
    proc.accent
        = std::make_unique<Process::ValueOutlet>("Accent", Id<Process::Port>(1), &proc);
  }

  if(auto port = obj.tryGet("Slide"))
  {
    JSONWriter writer{*port};
    proc.slide = Process::load_value_outlet(writer, &proc);
  }
  else
  {
    proc.slide
        = std::make_unique<Process::ValueOutlet>("Slide", Id<Process::Port>(2), &proc);
  }
  proc.m_channel = obj["Channel"].toInt();
  proc.m_currentPattern = obj["Pattern"].toInt();
  proc.m_patterns <<= obj["Patterns"];
}
