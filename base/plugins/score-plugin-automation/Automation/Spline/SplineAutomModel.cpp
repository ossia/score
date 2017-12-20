// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
  m_spline.points.push_back({0., 0.});

  m_spline.points.push_back({0.4, 0.075});
  m_spline.points.push_back({0.45,0.24});
  m_spline.points.push_back({0.5,0.5});

  m_spline.points.push_back({0.55,0.76});
  m_spline.points.push_back({0.7,0.9});
  m_spline.points.push_back({1.0, 1.0});

  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel()
{
}

QString ProcessModel::prettyName() const
{
  return address().toString();
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


/// Point ///
template <>
void DataStreamReader::read(
    const ossia::spline_point& autom)
{
  m_stream << autom.m_x << autom.m_y;
}

template <>
void DataStreamWriter::write(
    ossia::spline_point& autom)
{
  m_stream >> autom.m_x >> autom.m_y;
}
template <>
void JSONValueReader::read(
    const ossia::spline_point& autom)
{
  val = QJsonArray{autom.x(), autom.y()};
}

template <>
void JSONValueWriter::write(
    ossia::spline_point& autom)
{
  auto arr = val.toArray();
  autom.m_x = arr[0].toDouble();
  autom.m_y = arr[1].toDouble();
}

/// Data ///
template <>
void DataStreamReader::read(
    const ossia::spline_data& autom)
{
  m_stream << autom.points;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(
    ossia::spline_data& autom)
{
  m_stream >> autom.points;
  checkDelimiter();
}
template <>
void JSONValueReader::read(
    const ossia::spline_data& autom)
{
  val = toJsonValueArray(autom.points);
}
template <>
void JSONValueWriter::write(
    ossia::spline_data& autom)
{
  autom.points = fromJsonValueArray<std::vector<ossia::spline_point>>(val.toArray());
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
