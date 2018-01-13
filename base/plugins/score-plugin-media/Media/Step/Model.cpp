#include "Model.hpp"
#include <Process/Dataflow/Port.hpp>

namespace Media
{
namespace Step
{


Model::Model(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent):
  Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
, outlet{Process::make_outlet(Id<Process::Port>(0), this)}
{
  outlet->type = Process::PortType::Message;
  m_steps = {0.5f, 0.3f, 0.5f, 0.8f, 1.f, 0.f, 0.5f, 0.1f};
  m_stepDuration = 20000;
  m_stepCount = 8;
  m_min = -1.;
  m_max = 1.;
  metadata().setInstanceName(*this);
  init();
}

Model::~Model()
{

}

quint64 Model::stepCount() const { return m_stepCount; }

quint64 Model::stepDuration() const { return m_stepDuration; }

const std::vector<float>&Model::steps() const { return m_steps; }

double Model::min() const { return m_min; }

double Model::max() const { return m_max; }

void Model::setStepCount(quint64 s)
{
  if(s != m_stepCount)
  {
    m_stepCount = s;
    m_steps.resize(s);
    emit stepCountChanged(s);
  }
}

void Model::setStepDuration(quint64 s)
{
  if(s != m_stepDuration)
  {
    m_stepDuration = s;
    emit stepDurationChanged(s);
  }
}

void Model::setSteps(std::vector<float> v)
{
  if(m_steps != v)
  {
    m_steps = std::move(v);
    emit stepsChanged();
  }
}

void Model::setMin(double v)
{
  if(m_min != v)
  {
    m_min = v;
    emit minChanged(v);
  }
}

void Model::setMax(double v)
{
  if(m_max != v)
  {
    m_max = v;
    emit maxChanged(v);
  }
}


}
}
template <>
void DataStreamReader::read(const Media::Step::Model& proc)
{
  m_stream << *proc.outlet << proc.m_steps << proc.m_stepCount << proc.m_stepDuration << proc.m_min << proc.m_max;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Step::Model& proc)
{
  proc.outlet = Process::make_outlet(*this, &proc);
  m_stream >> proc.m_steps >> proc.m_stepCount >> proc.m_stepDuration >> proc.m_min >> proc.m_max;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Media::Step::Model& proc)
{
  obj["Outlet"] = toJsonObject(*proc.outlet);
  obj["Steps"] = toJsonValueArray(proc.m_steps);
  obj["StepCount"] = (qint64)proc.m_stepCount;
  obj["StepDur"] = (qint64)proc.m_stepDuration;
  obj["StepMin"] = (qint64)proc.m_min;
  obj["StepMax"] = (qint64)proc.m_max;
}

template <>
void JSONObjectWriter::write(Media::Step::Model& proc)
{
  {
    JSONObjectWriter writer{obj["Outlet"].toObject()};
    proc.outlet = Process::make_outlet(writer, &proc);
  }

  proc.m_steps = fromJsonValueArray<std::vector<float>>(obj["Steps"].toArray());
  proc.m_stepCount = obj["StepCount"].toInt();
  proc.m_stepDuration = obj["StepDur"].toInt();
  proc.m_min = obj["StepMin"].toDouble();
  proc.m_max = obj["StepMax"].toDouble();
}
