#include "TempoProcess.hpp"
#include <Process/Dataflow/Port.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::TempoProcess)
namespace Scenario
{

TempoProcess::TempoProcess(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
  : CurveProcessModel{duration,
                      id,
                      Metadata<ObjectKey_k, ProcessModel>::get(),
                      parent}
  , inlet{Process::make_value_inlet(Id<Process::Port>(0), this)}
{
  setCurve(new Curve::Model{Id<Curve::Model>(45345), this});

  auto s1 = new Curve::LinearSegment(
        Id<Curve::SegmentModel>(1), m_curve);

  s1->setStart({0., (120. - this->min) / (this->max - this->min)});
  s1->setEnd({1., (120. - this->min) / (this->max - this->min)});

  m_curve->addSegment(s1);

  metadata().setInstanceName(*this);
  init();
}

TempoProcess::~TempoProcess() {}

void TempoProcess::init()
{
  m_inlets.push_back(inlet.get());
}

QString TempoProcess::prettyName() const noexcept {
  return tr("Tempo");
}

QString TempoProcess::prettyValue(double x, double y) const noexcept
{
  return QString::number(y * (max - min) + min, 'f', 3);
}

void TempoProcess::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  // We only need to change the duration.
  setDuration(newDuration);
  m_curve->changed();
}

void TempoProcess::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
  // If there are no segments, nothing changes
  if (m_curve->segments().size() == 0)
  {
    setDuration(newDuration);
    return;
  }

  // Else, scale all the segments by the increase.
  double scale = duration() / newDuration;
  for (auto& segment : m_curve->segments())
  {
    Curve::Point pt = segment.start();
    pt.setX(pt.x() * scale);
    segment.setStart(pt);

    pt = segment.end();
    pt.setX(pt.x() * scale);
    segment.setEnd(pt);
  }

  setDuration(newDuration);
  m_curve->changed();
}

void TempoProcess::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
  // If there are no segments, nothing changes
  if (m_curve->segments().size() == 0)
  {
    setDuration(newDuration);
    return;
  }

  // Else, scale all the segments by the increase.
  double scale = duration() / newDuration;
  for (auto& segment : m_curve->segments())
  {
    Curve::Point pt = segment.start();
    pt.setX(pt.x() * scale);
    segment.setStart(pt);

    pt = segment.end();
    pt.setX(pt.x() * scale);
    segment.setEnd(pt);
  }

  setDuration(newDuration);
  m_curve->changed();
}

void TempoProcess::setCurve_impl() { }

}



template <>
void DataStreamReader::read(const Scenario::TempoProcess& autom)
{
  m_stream << *autom.inlet;
  readFrom(autom.curve());

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Scenario::TempoProcess& autom)
{
  autom.inlet = Process::load_value_inlet(*this, &autom);
  autom.setCurve(new Curve::Model{*this, &autom});

  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Scenario::TempoProcess& autom)
{
  obj["Inlet"] = toJsonObject(*autom.inlet);
  obj["Curve"] = toJsonObject(autom.curve());
}

template <>
void JSONObjectWriter::write(Scenario::TempoProcess& autom)
{
  {
    JSONObjectWriter writer{obj["Inlet"].toObject()};
    autom.inlet = Process::load_value_inlet(writer, &autom);
  }
  JSONObject::Deserializer curve_deser{obj["Curve"].toObject()};
  autom.setCurve(new Curve::Model{curve_deser, &autom});
}
