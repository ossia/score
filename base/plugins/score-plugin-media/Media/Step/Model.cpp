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
