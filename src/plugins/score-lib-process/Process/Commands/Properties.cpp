#include <Process/Commands/Properties.hpp>

#include <score/model/ModelMetadata.hpp>

namespace Process
{
RenameProcess::RenameProcess(const ProcessModel& process, QString name)
    : m_path{process}
    , m_old{process.metadata().getName()}
    , m_new{std::move(name)}
{
}

void RenameProcess::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).metadata().setName(m_old);
}

void RenameProcess::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).metadata().setName(m_new);
}

void RenameProcess::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_old << m_new;
}

void RenameProcess::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_old >> m_new;
}
}
