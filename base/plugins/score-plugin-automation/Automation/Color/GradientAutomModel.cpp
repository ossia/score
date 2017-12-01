// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Automation/Color/GradientAutomModel.hpp>
#include <Automation/Color/GradientAutomPresenter.hpp>
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <Process/Dataflow/Port.hpp>
#include <QColor>
namespace Gradient
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id,
                        Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , outlet{Process::make_outlet(Id<Process::Port>(0), this)}

{
  outlet->type = Process::PortType::Message;
  m_colors.insert(std::make_pair(0.2, QColor(Qt::black)));
  m_colors.insert(std::make_pair(0.8, QColor(Qt::white)));

  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel()
{
}

ProcessModel::ProcessModel(
    const ProcessModel& source,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{
        source, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , outlet{Process::clone_outlet(*source.outlet, this)}
    , m_colors{source.m_colors}
    , m_tween{source.m_tween}
{
  metadata().setInstanceName(*this);
}

QString ProcessModel::prettyName() const
{
  auto res = address().toString();
  if(!res.isEmpty())
    return res;
  return "Gradient";
}

const ProcessModel::gradient_colors&ProcessModel::gradient() const { return m_colors; }

void ProcessModel::setGradient(const ProcessModel::gradient_colors& c) {
  if(m_colors != c)
    {
      m_colors = c;
      emit gradientChanged();
    }
}

const ::State::AddressAccessor& ProcessModel::address() const
{
  return outlet->address();
}

bool ProcessModel::tween() const
{
  return m_tween;
}

void ProcessModel::setTween(bool tween)
{
  if (m_tween == tween)
    return;

  m_tween = tween;
  emit tweenChanged(tween);
}

Process::Inlets ProcessModel::inlets() const
{
  return {};
}

Process::Outlets ProcessModel::outlets() const
{
  return {outlet.get()};
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration)
{
  // We only need to change the duration.
  setDuration(newDuration);
}

void ProcessModel::setDurationAndGrow(const TimeVal& newDuration)
{
  setDuration(newDuration);
}

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration)
{
  setDuration(newDuration);
}

bool ProcessModel::contentHasDuration() const
{
  return true;
}

TimeVal ProcessModel::contentDuration() const
{
  auto lastPoint = 1.;
  if(!m_colors.empty())
  {
    auto back = m_colors.rbegin()->first;
    lastPoint = std::max(1., back);
  }

  return duration() * lastPoint;
}
}


template <>
void DataStreamReader::read(
    const Gradient::ProcessModel& autom)
{
  m_stream << autom.m_colors
           << autom.m_tween;
  insertDelimiter();
}


template <>
void DataStreamWriter::write(Gradient::ProcessModel& autom)
{
  m_stream >> autom.m_colors
           >> autom.m_tween;

  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Gradient::ProcessModel& autom)
{
  JSONValueReader v{}; v.readFrom(autom.m_colors);
  obj["Gradient"] = v.val;
  obj["Tween"] = autom.tween();
}


template <>
void JSONObjectWriter::write(Gradient::ProcessModel& autom)
{
  autom.setTween(obj["Tween"].toBool());
  JSONValueWriter v{}; v.val = obj["Gradient"];
  v.writeTo(autom.m_colors);
}
