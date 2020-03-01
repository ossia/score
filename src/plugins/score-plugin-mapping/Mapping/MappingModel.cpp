// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MappingModel.hpp"

#include <Curve/CurveModel.hpp>
#include <Curve/Process/CurveProcessModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <Mapping/MappingProcessMetadata.hpp>
#include <Process/Dataflow/Port.hpp>
#include <State/Address.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/tools/std/Optional.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Mapping::ProcessModel)
namespace Mapping
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Curve::CurveProcessModel{duration,
                               id,
                               Metadata<ObjectKey_k, ProcessModel>::get(),
                               parent}
    , inlet{Process::make_value_inlet(Id<Process::Port>(0), this)}
    , outlet{Process::make_value_outlet(Id<Process::Port>(0), this)}
    , m_sourceMin{0.}
    , m_sourceMax{1.}
    , m_targetMin{0.}
    , m_targetMax{1.}
{
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

ProcessModel::~ProcessModel() {}

void ProcessModel::init()
{
  inlet->setCustomData("In");
  outlet->setCustomData("Out");
  m_inlets.push_back(inlet.get());
  m_outlets.push_back(outlet.get());

  connect(
      inlet.get(),
      &Process::Port::addressChanged,
      this,
      [=](const State::AddressAccessor& arg) {
        sourceAddressChanged(arg);
        prettyNameChanged();
        m_curve->changed();
      });
  connect(
      outlet.get(),
      &Process::Port::addressChanged,
      this,
      [=](const State::AddressAccessor& arg) {
        targetAddressChanged(arg);
        prettyNameChanged();
        m_curve->changed();
      });
}

QString ProcessModel::prettyName() const noexcept
{
  QString str = sourceAddress().toString_unsafe() + " -> "
             + targetAddress().toString_unsafe();
  if (str != " -> ")
    return str;
  return tr("Mapping");
}

QString ProcessModel::prettyValue(double x, double y) const noexcept
{
  return QString::number(
             (x * (sourceMax() - sourceMin()) - sourceMin()), 'f', 3)
         + " -> "
         + QString::number(
               (y * (targetMax() - targetMin()) - targetMin()), 'f', 3);
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  // Whatever happens we want to keep the same curve.
  setDuration(newDuration);
  m_curve->changed();
}

void ProcessModel::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
  m_curve->changed();
}

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
  m_curve->changed();
}

State::AddressAccessor ProcessModel::sourceAddress() const noexcept
{
  return inlet->address();
}

double ProcessModel::sourceMin() const noexcept
{
  return m_sourceMin;
}

double ProcessModel::sourceMax() const noexcept
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
  sourceMinChanged(arg);
  m_curve->changed();
}

void ProcessModel::setSourceMax(double arg)
{
  if (m_sourceMax == arg)
    return;

  m_sourceMax = arg;
  sourceMaxChanged(arg);
  m_curve->changed();
}

State::AddressAccessor ProcessModel::targetAddress() const noexcept
{
  return outlet->address();
}

double ProcessModel::targetMin() const noexcept
{
  return m_targetMin;
}

double ProcessModel::targetMax() const noexcept
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
  targetMinChanged(arg);
  m_curve->changed();
}

void ProcessModel::setTargetMax(double arg)
{
  if (m_targetMax == arg)
    return;

  m_targetMax = arg;
  targetMaxChanged(arg);
  m_curve->changed();
}
}
