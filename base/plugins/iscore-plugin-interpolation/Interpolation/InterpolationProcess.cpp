#include "InterpolationProcess.hpp"
#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

namespace Interpolation
{
ProcessModel::~ProcessModel() = default;

ProcessState::ProcessState(
    ProcessModel& model, Point watchedPoint, QObject* parent)
    : ProcessStateDataInterface{model, parent}, m_point{watchedPoint}
{
}

ProcessModel& ProcessState::process() const
{
  return static_cast<ProcessModel&>(ProcessStateDataInterface::process());
}

State::Message ProcessState::message() const
{
  auto& proc = process();

  if (m_point == Point::Start)
  {
    return State::Message{proc.address(), proc.start()};
  }
  else if (m_point == Point::End)
  {
    return State::Message{proc.address(), proc.end()};
  }

  return {};
}

ProcessState::Point ProcessState::point() const
{
  return m_point;
}

ProcessState* ProcessState::clone(QObject* parent) const
{
  return new ProcessState{process(), m_point, parent};
}

std::vector<State::AddressAccessor> ProcessState::matchingAddresses()
{
  // TODO have a better check of "address validity"
  auto addr = process().address();
  if (!addr.address.device.isEmpty())
    return {std::move(addr)};
  return {};
}

State::MessageList ProcessState::messages() const
{
  /*
  if(!process().address().address.device.isEmpty())
  {
      auto mess = message();
      if(!mess.address.address.device.isEmpty())
          return {mess};
  }
  */

  return {};
}

State::MessageList ProcessState::setMessages(
    const State::MessageList& received, const Process::MessageNode&)
{
  auto& proc = process();
  State::AddressAccessor cur_address = proc.address();
  auto it = ossia::find_if(received, [&](const auto& mess) {
    return mess.address.address == cur_address.address
           && mess.address.qualifiers.get().accessors
                  == cur_address.qualifiers.get().accessors;
    // The unit is handled later.
  });
  if (it != received.end())
  {
    if (m_point == Point::Start)
    {
      proc.setStart(it->value);
    }
    else if (m_point == Point::End)
    {
      proc.setEnd(it->value);
    }
    proc.setSourceUnit(it->address.qualifiers.get().unit);
  }
  return messages();
}

ProcessModel::ProcessModel(
    const TimeValue& duration, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : CurveProcessModel{duration, id,
                        Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , m_startState{new ProcessState{*this, ProcessState::Start, this}}
    , m_endState{new ProcessState{*this, ProcessState::End, this}}
{
  // Named shall be enough ?
  setCurve(new Curve::Model{Id<Curve::Model>(45345), this});

  auto s1 = new Curve::DefaultCurveSegmentModel(
      Id<Curve::SegmentModel>(1), m_curve);
  s1->setStart({0., 0.0});
  s1->setEnd({1., 1.});

  m_curve->addSegment(s1);
  connect(m_curve, &Curve::Model::changed, this, &ProcessModel::curveChanged);

  metadata().setInstanceName(*this);
}

State::AddressAccessor ProcessModel::address() const
{
  return m_address;
}

const State::Unit& ProcessModel::sourceUnit() const
{
  return m_sourceUnit;
}

State::Value ProcessModel::start() const
{
  return m_start;
}

State::Value ProcessModel::end() const
{
  return m_end;
}

void ProcessModel::setAddress(const State::AddressAccessor& arg)
{
  if (m_address == arg)
  {
    return;
  }

  m_address = arg;
  emit addressChanged(arg);
  emit m_curve->changed();
}

void ProcessModel::setSourceUnit(const State::Unit& u)
{
  m_sourceUnit = u;
}

void ProcessModel::setStart(State::Value arg)
{
  if (m_start == arg)
    return;

  m_start = arg;
  emit startChanged(arg);
  emit m_curve->changed();
}

void ProcessModel::setEnd(State::Value arg)
{
  if (m_end == arg)
    return;

  m_end = arg;
  emit endChanged(arg);
  emit m_curve->changed();
}

QString ProcessModel::prettyName() const
{
  return address().toShortString();
}

void ProcessModel::setDurationAndScale(const TimeValue& newDuration)
{
  // We only need to change the duration.
  setDuration(newDuration);
  m_curve->changed();
}

void ProcessModel::setDurationAndGrow(const TimeValue& newDuration)
{
  // We only need to change the duration.
  setDuration(newDuration);
  m_curve->changed();
}

void ProcessModel::setDurationAndShrink(const TimeValue& newDuration)
{
  // We only need to change the duration.
  setDuration(newDuration);
  m_curve->changed();
}

ProcessState* ProcessModel::startStateData() const
{
  return m_startState;
}

ProcessState* ProcessModel::endStateData() const
{
  return m_endState;
}

ProcessModel::ProcessModel(
    const ProcessModel& source, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Curve::CurveProcessModel{source, id,
                               Metadata<ObjectKey_k, ProcessModel>::get(),
                               parent}
    , m_address(source.address())
    , m_sourceUnit(source.m_sourceUnit)
    , m_start{source.start()}
    , m_end{source.end()}
    , m_startState{new ProcessState{*this, ProcessState::Point::Start, this}}
    , m_endState{new ProcessState{*this, ProcessState::Point::End, this}}
{
  setCurve(source.curve().clone(source.curve().id(), this));
  connect(m_curve, &Curve::Model::changed, this, &ProcessModel::curveChanged);
  metadata().setInstanceName(*this);
  // TODO instead make a copy constructor in CurveProcessModel
}
}

template <>
ISCORE_PLUGIN_INTERPOLATION_EXPORT void
Visitor<Reader<DataStream>>::read(
    const Interpolation::ProcessModel& interp)
{
  readFrom(interp.curve());

  m_stream << interp.address() << interp.sourceUnit() << interp.start()
           << interp.end();

  insertDelimiter();
}

template <>
ISCORE_PLUGIN_INTERPOLATION_EXPORT void
Visitor<Writer<DataStream>>::writeTo(Interpolation::ProcessModel& interp)
{
  interp.setCurve(new Curve::Model{*this, &interp});

  State::AddressAccessor address;
  ossia::unit_t u;
  State::Value start, end;

  m_stream >> address >> u >> start >> end;

  interp.setAddress(address);
  interp.setSourceUnit(u);
  interp.setStart(start);
  interp.setEnd(end);

  checkDelimiter();
}

template <>
ISCORE_PLUGIN_INTERPOLATION_EXPORT void
Visitor<Reader<JSONObject>>::readFromConcrete(
    const Interpolation::ProcessModel& interp)
{
  m_obj["Curve"] = toJsonObject(interp.curve());
  m_obj[strings.Address] = toJsonObject(interp.address());
  m_obj[strings.Unit] = QString::fromStdString(
      ossia::get_pretty_unit_text(interp.sourceUnit()));
  m_obj[strings.Start] = toJsonObject(interp.start());
  m_obj[strings.End] = toJsonObject(interp.end());
}

template <>
ISCORE_PLUGIN_INTERPOLATION_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(Interpolation::ProcessModel& interp)
{
  Deserializer<JSONObject> curve_deser{m_obj["Curve"].toObject()};
  interp.setCurve(new Curve::Model{curve_deser, &interp});

  interp.setAddress(
      fromJsonObject<State::AddressAccessor>(m_obj[strings.Address]));
  interp.setSourceUnit(
      ossia::parse_pretty_unit(m_obj[strings.Unit].toString().toStdString()));
  interp.setStart(fromJsonObject<State::Value>(m_obj[strings.Start]));
  interp.setEnd(fromJsonObject<State::Value>(m_obj[strings.End]));
}
