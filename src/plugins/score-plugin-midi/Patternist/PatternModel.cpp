// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortSerialization.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/tools/File.hpp>

#include <ossia/detail/math.hpp>
#include <ossia/detail/ssize.hpp>

#include <Patternist/PatternModel.hpp>
#include <Patternist/PatternParsing.hpp>

#include <QWidget>

#include <cmath>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Patternist::ProcessModel)

namespace Patternist
{
void normalize(Pattern& p) noexcept
{
  p.length = std::clamp(p.length, 1, Pattern::maxLength);
  p.division = std::max(p.division, 1);
  for(Lane& lane : p.lanes)
  {
    lane.length = std::clamp(lane.length, 0, Pattern::maxLength);
    lane.division = std::max(lane.division, 0);
    lane.velocity = std::clamp(lane.velocity, 0.f, 1.f);

    const int n = std::max(p.length, lane.length);
    if(std::ssize(lane.pattern) < n)
      lane.pattern.resize(n, Note::Rest);
    lane.params.resize(lane.pattern.size());
    for(StepParams& s : lane.params)
    {
      s.velocity = std::clamp(s.velocity, 0.f, 1.f);
      s.chance = std::clamp(s.chance, 0.f, 1.f);
      s.offset = std::clamp(s.offset, -0.5f, 0.5f);
      s.gate = std::clamp(s.gate, 0.f, 1.f);
      s.ratchet = std::clamp<uint8_t>(s.ratchet, 1, 8);
      s.velocityRandom = std::clamp(s.velocityRandom, 0.f, 1.f);
      s.pitch = std::clamp<int8_t>(s.pitch, -48, 48);
      s.pitchRandom = std::clamp<uint8_t>(s.pitchRandom, 0, 24);
      s.cycleB = std::clamp<uint8_t>(s.cycleB, 1, 16);
      s.cycleA = std::clamp<uint8_t>(s.cycleA, 1, s.cycleB);
    }
  }
}

static std::unique_ptr<Process::ControlInlet> makeFill(QObject* parent)
{
  return std::make_unique<Process::Toggle>(
      false, QObject::tr("Fill"), Id<Process::Port>(2), parent);
}

static std::unique_ptr<Process::ControlInlet> makeDensity(QObject* parent)
{
  return std::make_unique<Process::FloatSlider>(
      0.f, 1.f, 1.f, QObject::tr("Density"), Id<Process::Port>(3), parent);
}

static std::unique_ptr<Process::ControlInlet> makeVelocity(QObject* parent)
{
  return std::make_unique<Process::FloatSlider>(
      0.f, 1.f, 1.f, QObject::tr("Velocity"), Id<Process::Port>(4), parent);
}

static std::unique_ptr<Process::ControlInlet> makeSwing(QObject* parent)
{
  return std::make_unique<Process::FloatSlider>(
      0.f, 1.f, 0.f, QObject::tr("Swing"), Id<Process::Port>(5), parent);
}

static std::unique_ptr<Process::ControlInlet> makeRatchet(QObject* parent)
{
  return std::make_unique<Process::IntSlider>(
      1, 8, 1, QObject::tr("Ratchet"), Id<Process::Port>(6), parent);
}

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
      0, ProcessModel::maxPatterns - 1, 0, QObject::tr("Pattern"), Id<Process::Port>(0),
      parent);
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
    : Process::ProcessModel{
          duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , patternSelect{makePatternSelect(this)}
    , switchQuantification{makeSwitchQuantification(this)}
    , fill{makeFill(this)}
    , density{makeDensity(this)}
    , velocity{makeVelocity(this)}
    , swing{makeSwing(this)}
    , ratchet{makeRatchet(this)}
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
  normalize(pattern);
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
      {
        for(auto& p : pat)
          normalize(p);
        this->m_patterns = std::move(pat);
      }
}

void ProcessModel::init()
{
  m_inlets.push_back(patternSelect.get());
  m_inlets.push_back(switchQuantification.get());
  m_inlets.push_back(fill.get());
  m_inlets.push_back(density.get());
  m_inlets.push_back(velocity.get());
  m_inlets.push_back(swing.get());
  m_inlets.push_back(ratchet.get());
  for(auto* port :
      {fill.get(), density.get(), velocity.get(), swing.get(), ratchet.get()})
    port->displayHandledExplicitly = false;

  // The pattern grid is the whole body: no widget shows these controls there,
  // so the node shows them as ports, folded or not (documents saved before
  // carry them as "Hidden").
  patternSelect->displayHandledExplicitly = false;
  switchQuantification->displayHandledExplicitly = false;
  patternSelect->visibleWhenFolded = true;
  switchQuantification->visibleWhenFolded = true;

  // A document written before this port existed carries the pattern in the
  // property alone: seed the port from it before listening, or the selector
  // would show 0 while another pattern plays.
  if(ossia::convert<int>(patternSelect->value()) != m_currentPattern)
    patternSelect->setValue(m_currentPattern);

  // The port is the only selector: the property follows it, so the layer keeps
  // drawing the pattern the port picked. Execution reads the port itself, so an
  // automated value never comes back through here to grow the list.
  connect(
      patternSelect.get(), &Process::ControlInlet::valueChanged, this,
      [this](const ossia::value& v) { setCurrentPattern(ossia::convert<int>(v)); });
  m_outlets.push_back(outlet.get());
  m_outlets.push_back(accent.get());
  m_outlets.push_back(slide.get());
}

ProcessModel::~ProcessModel()
{
  // The editor draws from this model: it cannot outlive it, not even until a
  // deferred deletion.
  if(auto ui = externalUI)
  {
    externalUI = nullptr;
    delete ui;
  }
}

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
  // The port's own range: a remote-control write reaches setValue without any
  // clamp, and growing the list to meet it would allocate without bound.
  n = std::clamp(n, 0, maxPatterns - 1);
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

  // Keep the port on the pattern that is actually current, so that what is
  // saved, what the grid draws and what plays cannot drift apart. The port
  // writes back here, which stops as soon as the two agree.
  if(ossia::convert<int>(patternSelect->value()) != m_currentPattern)
    patternSelect->setValue(m_currentPattern);
}

int ProcessModel::currentPattern() const noexcept
{
  return m_currentPattern;
}

void ProcessModel::setLaneLabels(LaneLabels l)
{
  if(l != m_laneLabels)
  {
    m_laneLabels = l;
    laneLabelsChanged(l);
  }
}

LaneLabels ProcessModel::laneLabels() const noexcept
{
  return m_laneLabels;
}

void ProcessModel::setPattern(int n, Pattern p)
{
  if(!ossia::valid_index(n, m_patterns))
    return;
  normalize(p);
  m_patterns[n] = std::move(p);
  patternsChanged();
}

void ProcessModel::setPatterns(const std::vector<Pattern>& n)
{
  if(n != m_patterns)
  {
    m_patterns = n;
    for(auto& p : m_patterns)
      normalize(p);
    patternsChanged();
  }
}

void ProcessModel::ancestorStartDateChanged()
{
  timeGridChanged();
}

void ProcessModel::ancestorTempoChanged()
{
  timeGridChanged();
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
void DataStreamReader::read(const Patternist::StepParams& s)
{
  m_stream << s.velocity << s.velocityRandom << s.chance << s.offset << s.gate
           << s.ratchet << s.pitch << s.pitchRandom << s.condition << s.cycleA << s.cycleB;
}

template <>
void DataStreamWriter::write(Patternist::StepParams& s)
{
  m_stream >> s.velocity >> s.velocityRandom >> s.chance >> s.offset >> s.gate
      >> s.ratchet >> s.pitch >> s.pitchRandom >> s.condition >> s.cycleA >> s.cycleB;
}

template <>
void DataStreamReader::read(const Patternist::Lane& proc)
{
  m_stream << proc.note << proc.pattern << proc.mute << proc.solo << proc.length
           << proc.division << proc.velocity << proc.params;
}

template <>
void DataStreamWriter::write(Patternist::Lane& proc)
{
  m_stream >> proc.note >> proc.pattern >> proc.mute >> proc.solo >> proc.length
      >> proc.division >> proc.velocity >> proc.params;
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
  if(proc.mute)
    obj["Mute"] = true;
  if(proc.solo)
    obj["Solo"] = true;
  if(proc.length > 0)
    obj["Length"] = proc.length;
  if(proc.division > 0)
    obj["Division"] = proc.division;
  if(proc.velocity != Patternist::Lane{}.velocity)
    obj["Velocity"] = proc.velocity;

  // Only the steps that differ from the defaults.
  const bool anyParams = std::any_of(
      proc.params.begin(), proc.params.end(),
      [](const Patternist::StepParams& s) { return !s.isDefault(); });
  if(anyParams)
  {
    const Patternist::StepParams def;
    stream.Key("Steps");
    stream.StartArray();
    for(std::size_t i = 0; i < proc.params.size(); i++)
    {
      const auto& s = proc.params[i];
      if(s.isDefault())
        continue;
      stream.StartObject();
      obj["Step"] = (int)i;
      if(s.velocity != def.velocity)
        obj["Velocity"] = s.velocity;
      if(s.velocityRandom != def.velocityRandom)
        obj["VelocityRandom"] = s.velocityRandom;
      if(s.chance != def.chance)
        obj["Chance"] = s.chance;
      if(s.offset != def.offset)
        obj["Offset"] = s.offset;
      if(s.gate != def.gate)
        obj["Gate"] = s.gate;
      if(s.ratchet != def.ratchet)
        obj["Ratchet"] = (int)s.ratchet;
      if(s.pitch != def.pitch)
        obj["Pitch"] = (int)s.pitch;
      if(s.pitchRandom != def.pitchRandom)
        obj["PitchRandom"] = (int)s.pitchRandom;
      if(s.condition != def.condition)
      {
        obj["Condition"] = (int)s.condition;
        obj["CycleA"] = (int)s.cycleA;
        obj["CycleB"] = (int)s.cycleB;
      }
      stream.EndObject();
    }
    stream.EndArray();
  }
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
  if(auto v = obj.tryGet("Mute"))
    proc.mute = v->toBool();
  if(auto v = obj.tryGet("Solo"))
    proc.solo = v->toBool();
  if(auto v = obj.tryGet("Length"))
    proc.length = v->toInt();
  if(auto v = obj.tryGet("Division"))
    proc.division = v->toInt();
  if(auto v = obj.tryGet("Velocity"))
    proc.velocity = v->toDouble();

  proc.params.resize(proc.pattern.size());
  if(auto steps = obj.tryGet("Steps"))
  {
    for(const auto& v : steps->toArray())
    {
      if(!v.IsObject() || !v.HasMember("Step") || !v["Step"].IsInt())
        continue;
      const int i = v["Step"].GetInt();
      if(i < 0 || i >= Patternist::Pattern::maxLength)
        continue;
      if(i >= std::ssize(proc.params))
        proc.params.resize(i + 1);
      auto& s = proc.params[i];
      const auto num = [&](const char* key, auto& field) {
        if(auto it = v.FindMember(key); it != v.MemberEnd() && it->value.IsNumber())
          field = it->value.GetDouble();
      };
      num("Velocity", s.velocity);
      num("VelocityRandom", s.velocityRandom);
      num("Chance", s.chance);
      num("Offset", s.offset);
      num("Gate", s.gate);
      int ratchet = s.ratchet, pitch = s.pitch, pitchRandom = s.pitchRandom, cond = 0,
          a = s.cycleA, b = s.cycleB;
      num("Ratchet", ratchet);
      num("Pitch", pitch);
      num("PitchRandom", pitchRandom);
      num("Condition", cond);
      num("CycleA", a);
      num("CycleB", b);
      s.ratchet = std::clamp(ratchet, 1, 8);
      s.pitch = std::clamp(pitch, -48, 48);
      s.pitchRandom = std::clamp(pitchRandom, 0, 24);
      s.velocityRandom = std::clamp(s.velocityRandom, 0.f, 1.f);
      s.condition = (Patternist::Condition)std::clamp(
          cond, 0, (int)Patternist::Condition::Cycle);
      s.cycleA = std::clamp(a, 1, 16);
      s.cycleB = std::clamp(b, 1, 16);
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
  Patternist::normalize(proc);
}

template <>
void DataStreamReader::read(const Patternist::ProcessModel& proc)
{
  m_stream << *proc.patternSelect << *proc.switchQuantification << *proc.fill
           << *proc.density << *proc.velocity << *proc.swing << *proc.ratchet
           << *proc.outlet
           << *proc.accent << *proc.slide << proc.m_channel << proc.m_currentPattern
           << proc.m_laneLabels << proc.m_patterns;

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Patternist::ProcessModel& proc)
{
  proc.patternSelect = Process::load_control_inlet(*this, &proc);
  proc.switchQuantification = Process::load_control_inlet(*this, &proc);
  proc.fill = Process::load_control_inlet(*this, &proc);
  proc.density = Process::load_control_inlet(*this, &proc);
  proc.velocity = Process::load_control_inlet(*this, &proc);
  proc.swing = Process::load_control_inlet(*this, &proc);
  proc.ratchet = Process::load_control_inlet(*this, &proc);
  proc.outlet = Process::load_midi_outlet(*this, &proc);
  proc.accent = Process::load_value_outlet(*this, &proc);
  proc.slide = Process::load_value_outlet(*this, &proc);
  m_stream >> proc.m_channel >> proc.m_currentPattern >> proc.m_laneLabels
      >> proc.m_patterns;

  checkDelimiter();
}

template <>
void JSONReader::read(const Patternist::ProcessModel& proc)
{
  obj["PatternSelect"] = *proc.patternSelect;
  obj["SwitchQuantification"] = *proc.switchQuantification;
  obj["Fill"] = *proc.fill;
  obj["Density"] = *proc.density;
  obj["Velocity"] = *proc.velocity;
  obj["Swing"] = *proc.swing;
  obj["Ratchet"] = *proc.ratchet;
  obj["Outlet"] = *proc.outlet;
  obj["Accent"] = *proc.accent;
  obj["Slide"] = *proc.slide;
  obj["Channel"] = proc.m_channel;
  obj["Pattern"] = proc.m_currentPattern;
  obj["LaneLabels"] = (int)proc.m_laneLabels;
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

  const auto loadControl = [&](const std::string& key, auto make) {
    if(auto port = obj.tryGet(key))
    {
      JSONWriter writer{*port};
      return Process::load_control_inlet(writer, &proc);
    }
    return make(&proc);
  };
  proc.fill = loadControl("Fill", Patternist::makeFill);
  proc.density = loadControl("Density", Patternist::makeDensity);
  proc.velocity = loadControl("Velocity", Patternist::makeVelocity);
  proc.swing = loadControl("Swing", Patternist::makeSwing);
  proc.ratchet = loadControl("Ratchet", Patternist::makeRatchet);

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
  if(auto v = obj.tryGet("LaneLabels"))
    proc.m_laneLabels = (Patternist::LaneLabels)std::clamp(
        v->toInt(), 0, (int)Patternist::LaneLabels::DrumName);
  proc.m_patterns <<= obj["Patterns"];
}
