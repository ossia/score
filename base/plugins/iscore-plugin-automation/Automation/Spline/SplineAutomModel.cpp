#include <Automation/Spline/SplineAutomModel.hpp>
#include <Automation/Spline/SplineAutomPresenter.hpp>
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <QColor>
namespace Spline
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id,
                        Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
  m_spline.points.push_back(QPointF(0., 0.));

  m_spline.points.push_back(QPointF(0.4, 0.075));
  m_spline.points.push_back(QPointF(0.45,0.24));
  m_spline.points.push_back(QPointF(0.5,0.5));

  m_spline.points.push_back(QPointF(0.55,0.76));
  m_spline.points.push_back(QPointF(0.7,0.9));
  m_spline.points.push_back(QPointF(1.0, 1.0));

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
    , m_address(source.m_address)
    , m_spline{source.m_spline}
    , m_tween{source.m_tween}
{
}

QString ProcessModel::prettyName() const
{
  return address().toShortString();
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
  return duration();
}

::State::AddressAccessor ProcessModel::address() const
{
  return m_address;
}

void ProcessModel::setAddress(const ::State::AddressAccessor& arg)
{
  if (m_address == arg)
  {
    return;
  }

  m_address = arg;
  emit addressChanged(arg);
  emit unitChanged(arg.qualifiers.get().unit);
}

State::Unit ProcessModel::unit() const
{
  return m_address.qualifiers.get().unit;
}

void ProcessModel::setUnit(const State::Unit& u)
{
  if (u != unit())
  {
    m_address.qualifiers.get().unit = u;
    emit addressChanged(m_address);
    emit unitChanged(u);
  }
}
}


template <>
void DataStreamReader::read(
    const Spline::spline_data& autom)
{
  insertDelimiter();
}


template <>
void DataStreamWriter::write(
    Spline::spline_data& autom)
{
  checkDelimiter();
}
template <>
void JSONValueReader::read(
    const Spline::spline_data& autom)
{

}
template <>
void JSONValueWriter::write(
    Spline::spline_data& autom)
{

}



template <>
void DataStreamReader::read(const Spline::ProcessModel& autom)
{
  m_stream << autom.m_address
           << autom.m_spline
           << autom.m_tween;

  insertDelimiter();
}
template <>
void DataStreamWriter::write(Spline::ProcessModel& autom)
{
  m_stream >> autom.m_address
           >> autom.m_spline
           >> autom.m_tween;

  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Spline::ProcessModel& autom)
{
  obj[strings.Address] = toJsonObject(autom.address());
  JSONValueReader v{}; v.readFrom(autom.m_spline);
  obj["Spline"] = v.val;
  obj["Tween"] = autom.tween();
}


template <>
void JSONObjectWriter::write(Spline::ProcessModel& autom)
{
  autom.setAddress(
      fromJsonObject<State::AddressAccessor>(obj[strings.Address]));
  autom.setTween(obj["Tween"].toBool());
  JSONValueWriter v{}; v.val = obj["Spline"];
  v.writeTo(autom.m_spline);
}
