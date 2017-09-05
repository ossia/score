// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/std/Optional.hpp>

#include "MappingModel.hpp"
#include <Curve/Process/CurveProcessModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Mapping/MappingProcessMetadata.hpp>
#include <State/Address.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/model/Identifier.hpp>

namespace Mapping
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Curve::CurveProcessModel{
          duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
  setCurve(new Curve::Model{Id<Curve::Model>(45345), this});

  auto s1 = new Curve::DefaultCurveSegmentModel(
      Id<Curve::SegmentModel>(1), m_curve);
  s1->setStart({0., 0.0});
  s1->setEnd({1., 1.});

  m_curve->addSegment(s1);
  connect(m_curve, &Curve::Model::changed, this, &ProcessModel::curveChanged);

  metadata().setInstanceName(*this);
}

ProcessModel::ProcessModel(
    const ProcessModel& source,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : CurveProcessModel{source, id, Metadata<ObjectKey_k, ProcessModel>::get(),
                        parent}
    , m_sourceAddress(source.sourceAddress())
    , m_targetAddress(source.targetAddress())
    , m_sourceMin{source.sourceMin()}
    , m_sourceMax{source.sourceMax()}
    , m_targetMin{source.targetMin()}
    , m_targetMax{source.targetMax()}
{
  setCurve(source.curve().clone(source.curve().id(), this));
  connect(m_curve, &Curve::Model::changed, this, &ProcessModel::curveChanged);
  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel()
{
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
  return m_sourceAddress;
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
  if (m_sourceAddress == arg)
  {
    return;
  }

  m_sourceAddress = arg;
  emit sourceAddressChanged(arg);
  emit prettyNameChanged();
  emit m_curve->changed();
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
  return m_targetAddress;
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
  if (m_targetAddress == arg)
  {
    return;
  }

  m_targetAddress = arg;
  emit targetAddressChanged(arg);
  emit prettyNameChanged();
  emit m_curve->changed();
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
