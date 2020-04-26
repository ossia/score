#include "Model.hpp"

#include <Process/Dataflow/Port.hpp>

#include <ossia/detail/pod_vector.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::Step::Model)
namespace Media
{
namespace Step
{

Model::Model(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration,
                            id,
                            Metadata<ObjectKey_k, ProcessModel>::get(),
                            parent}
    , outlet{Process::make_value_outlet(Id<Process::Port>(0), this)}
{
  m_steps = {0.5f, 0.3f, 0.5f, 0.8f, 1.f, 0.f, 0.5f, 0.1f};
  m_stepDuration = 20000;
  m_stepCount = 8;
  m_min = -1.;
  m_max = 1.;
  metadata().setInstanceName(*this);
  init();
}

Model::~Model() {}

int Model::stepCount() const
{
  return m_stepCount;
}

int Model::stepDuration() const
{
  return m_stepDuration;
}

const ossia::float_vector& Model::steps() const
{
  return m_steps;
}

double Model::min() const
{
  return m_min;
}

double Model::max() const
{
  return m_max;
}

void Model::setStepCount(int s)
{
  if (s != m_stepCount)
  {
    m_stepCount = s;
    m_steps.resize(s);
    stepCountChanged(s);
  }
}

void Model::setStepDuration(int s)
{
  if (s != m_stepDuration)
  {
    m_stepDuration = s;
    stepDurationChanged(s);
  }
}

void Model::setSteps(ossia::float_vector v)
{
  if (m_steps != v)
  {
    m_steps = std::move(v);
    stepsChanged();
  }
}

void Model::setMin(double v)
{
  if (m_min != v)
  {
    m_min = v;
    minChanged(v);
  }
}

void Model::setMax(double v)
{
  if (m_max != v)
  {
    m_max = v;
    maxChanged(v);
  }
}
}
}
template <>
void DataStreamReader::read(const Media::Step::Model& proc)
{
  m_stream << *proc.outlet << proc.m_steps << proc.m_stepCount
           << proc.m_stepDuration << proc.m_min << proc.m_max;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Step::Model& proc)
{
  proc.outlet = Process::load_value_outlet(*this, &proc);
  m_stream >> proc.m_steps >> proc.m_stepCount >> proc.m_stepDuration
      >> proc.m_min >> proc.m_max;
  checkDelimiter();
}

template <>
void JSONReader::read(const Media::Step::Model& proc)
{
  obj["Outlet"] = *proc.outlet;
  obj["Steps"] = proc.m_steps;
  obj["StepCount"] = proc.m_stepCount;
  obj["StepDur"] = proc.m_stepDuration;
  obj["StepMin"] = proc.m_min;
  obj["StepMax"] = proc.m_max;
}

template <>
void JSONWriter::write(Media::Step::Model& proc)
{
  {
    JSONWriter writer{obj["Outlet"]};
    proc.outlet = Process::load_value_outlet(writer, &proc);
  }

  proc.m_steps <<= obj["Steps"];
  proc.m_stepCount = obj["StepCount"].toInt();
  proc.m_stepDuration = obj["StepDur"].toInt();
  proc.m_min = obj["StepMin"].toDouble();
  proc.m_max = obj["StepMax"].toDouble();
}
