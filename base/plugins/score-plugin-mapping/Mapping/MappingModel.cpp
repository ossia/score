// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/tools/std/Optional.hpp>

#include <Process/Dataflow/Port.hpp>
#include "MappingModel.hpp"
#include <Curve/Process/CurveProcessModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Mapping/MappingProcessMetadata.hpp>
#include <State/Address.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/model/Identifier.hpp>

namespace Mapping
{
Process::Inlets ProcessModel::inlets() const
{
  return {inlet.get()};
}

Process::Outlets ProcessModel::outlets() const
{
  return {outlet.get()};
}

ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Curve::CurveProcessModel{
          duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , inlet{Process::make_inlet(Id<Process::Port>(0), this)}
    , outlet{Process::make_outlet(Id<Process::Port>(0), this)}
    , m_sourceMin{0.}
    , m_sourceMax{1.}
    , m_targetMin{0.}
    , m_targetMax{1.}
{
  inlet->type = Process::PortType::Message;
  outlet->type = Process::PortType::Message;

  setCurve(new Curve::Model{Id<Curve::Model>(45345), this});

  auto s1 = new Curve::DefaultCurveSegmentModel(
      Id<Curve::SegmentModel>(1), m_curve);
  s1->setStart({0., 0.0});
  s1->setEnd({1., 1.});

  m_curve->addSegment(s1);

  metadata().setInstanceName(*this);

  init();
}

ProcessModel::ProcessModel(JSONObject::Deserializer& vis, QObject* parent)
  : CurveProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

ProcessModel::ProcessModel(DataStream::Deserializer& vis, QObject* parent)
  : CurveProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

ProcessModel::~ProcessModel()
{
}

void ProcessModel::init()
{

  connect(inlet.get(), &Process::Port::addressChanged,
          this, [=] (const State::AddressAccessor& arg) {
    emit sourceAddressChanged(arg);
    emit prettyNameChanged();
    emit m_curve->changed();
  });
  connect(outlet.get(), &Process::Port::addressChanged,
          this, [=] (const State::AddressAccessor& arg) {
    emit targetAddressChanged(arg);
    emit prettyNameChanged();
    emit m_curve->changed();
  });
}

QString ProcessModel::prettyName() const
{
  return sourceAddress().toString()
         + " -> " + targetAddress().toString();
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration)
{
  // Whatever happens we want to keep the same curve.
  setDuration(newDuration);
  m_curve->changed();
}

void ProcessModel::setDurationAndGrow(const TimeVal& newDuration)
{
  setDuration(newDuration);
  m_curve->changed();
}

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration)
{
  setDuration(newDuration);
  m_curve->changed();
}

State::AddressAccessor ProcessModel::sourceAddress() const
{
  return inlet->address();
}

double ProcessModel::sourceMin() const
{
  return m_sourceMin;
}

double ProcessModel::sourceMax() const
{
  return m_sourceMax;
}

void ProcessModel::setSourceAddress(const State::AddressAccessor& arg)
{
  inlet->setAddress(arg);
}

void ProcessModel::setSourceMin(double arg)
{
  if (m_sourceMin == arg)
    return;

  m_sourceMin = arg;
  emit sourceMinChanged(arg);
  emit m_curve->changed();
}

void ProcessModel::setSourceMax(double arg)
{
  if (m_sourceMax == arg)
    return;

  m_sourceMax = arg;
  emit sourceMaxChanged(arg);
  emit m_curve->changed();
}

State::AddressAccessor ProcessModel::targetAddress() const
{
  return outlet->address();
}

double ProcessModel::targetMin() const
{
  return m_targetMin;
}

double ProcessModel::targetMax() const
{
  return m_targetMax;
}

void ProcessModel::setTargetAddress(const State::AddressAccessor& arg)
{
  outlet->setAddress(arg);
}

void ProcessModel::setTargetMin(double arg)
{
  if (m_targetMin == arg)
    return;

  m_targetMin = arg;
  emit targetMinChanged(arg);
  emit m_curve->changed();
}

void ProcessModel::setTargetMax(double arg)
{
  if (m_targetMax == arg)
    return;

  m_targetMax = arg;
  emit targetMaxChanged(arg);
  emit m_curve->changed();
}
}
