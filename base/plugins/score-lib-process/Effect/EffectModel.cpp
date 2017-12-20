#include "EffectModel.hpp"
namespace Process
{
EffectModel::EffectModel(
        const Id<EffectModel>& id,
        QObject* parent):
    Entity{id, staticMetaObject.className(), parent}
{
    metadata().setInstanceName(*this);
}

EffectModel::~EffectModel()
{

}

void EffectModel::showUI()
{

}

void EffectModel::hideUI()
{

}

Process::Inlet* EffectModel::inlet(const Id<Process::Port>& p) const
{
  for(auto e : m_inlets)
    if(e->id() == p)
      return e;
  return nullptr;
}

Process::Outlet* EffectModel::outlet(const Id<Process::Port>& p) const
{
  for(auto e : m_outlets)
    if(e->id() == p)
      return e;
  return nullptr;
}

}

template <>
void DataStreamReader::read(
        const Process::EffectModel& eff)
{
}

template <>
void DataStreamWriter::write(
        Process::EffectModel& eff)
{
}

template <>
void JSONObjectReader::read(
        const Process::EffectModel& eff)
{
}

template <>
void JSONObjectWriter::write(
        Process::EffectModel& eff)
{
}
