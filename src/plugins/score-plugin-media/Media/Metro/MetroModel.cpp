#include "MetroModel.hpp"

#include <Process/Dataflow/Port.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::Metro::Model)

namespace Media::Metro
{

Model::Model(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration,
                            id,
                            Metadata<ObjectKey_k, ProcessModel>::get(),
                            parent}
    , outlet{Process::make_outlet(Id<Process::Port>(0), this)}
{
  outlet->type = Process::PortType::Audio;
  outlet->setPropagate(true);
  metadata().setInstanceName(*this);
  init();
}

Model::~Model() {}

}
template <>
void DataStreamReader::read(const Media::Metro::Model& proc)
{
  m_stream << *proc.outlet;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Metro::Model& proc)
{
  proc.outlet = Process::make_outlet(*this, &proc);
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Media::Metro::Model& proc)
{
  obj["Outlet"] = toJsonObject(*proc.outlet);
}

template <>
void JSONObjectWriter::write(Media::Metro::Model& proc)
{
  {
    JSONObjectWriter writer{obj["Outlet"].toObject()};
    proc.outlet = Process::make_outlet(writer, &proc);
  }
}
