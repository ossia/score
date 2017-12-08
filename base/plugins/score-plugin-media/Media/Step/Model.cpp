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
  m_steps = {0.5, 0.3, 0.5, 0.8, 1., 0., 0.5, 0.1};
  m_stepDuration = 20000;
  m_stepCount = 8;
  m_min = -1.;
  m_max = 1.;
  metadata().setInstanceName(*this);
}

Model::Model(
    const Model& source,
    const Id<Process::ProcessModel>& id,
    QObject* parent):
  Process::ProcessModel{
      source,
      id,
      Metadata<ObjectKey_k, ProcessModel>::get(),
      parent}
, outlet{Process::clone_outlet(*source.outlet, this)}
{
  metadata().setInstanceName(*this);
}

Model::~Model()
{

}

Process::Inlets Model::inlets() const
{
  return {};
}

Process::Outlets Model::outlets() const
{
  return {outlet.get()};
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
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Step::Model& proc)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Media::Step::Model& proc)
{
}

template <>
void JSONObjectWriter::write(Media::Step::Model& proc)
{
}
