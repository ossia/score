#include "InsertEffect.hpp"

#include <score/plugins/SerializableHelpers.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/document/ChangeId.hpp>
namespace Media
{
InsertEffect::InsertEffect(
    const Media::ChainProcess& model,
    const UuidKey<Process::ProcessModel>& effectKind,
    QString data,
    std::size_t effectPos)
    : m_model{model}
    , m_id{getStrongId(model.effects())}
    , m_effectKind{effectKind}
    , m_data{data}
    , m_pos{effectPos}
{
}

void InsertEffect::undo(const score::DocumentContext& ctx) const
{
  auto& process = m_model.find(ctx);
  if (ossia::find(process.effects(), m_id) != process.effects().end())
    process.removeEffect(m_id);
}

void InsertEffect::redo(const score::DocumentContext& ctx) const
{
  auto& process = m_model.find(ctx);
  auto& fact_list = ctx.app.interfaces<Process::ProcessFactoryList>();

  if (auto fact = fact_list.get(m_effectKind))
  {
    auto model = fact->make(TimeVal::zero(), m_data, m_id, ctx, &process);
    process.insertEffect(model, m_pos);
  }
  else
  {
    SCORE_TODO;
    // Insert a fake effect ?
  }
}

void InsertEffect::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_id << m_effectKind << m_data << m_pos;
}

void InsertEffect::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_id >> m_effectKind >> m_data >> m_pos;
}

LoadEffect::LoadEffect(
    const Media::ChainProcess& model,
    const QJsonObject& data,
    std::size_t effectPos)
    : m_path{model}
    , m_id{getStrongId(model.effects())}
    , m_data{data}
    , m_pos{effectPos}
{
}

void LoadEffect::undo(const score::DocumentContext& ctx) const
{
  auto& process = m_path.find(ctx);
  if (ossia::find(process.effects(), m_id) != process.effects().end())
    process.removeEffect(m_id);
}

void LoadEffect::redo(const score::DocumentContext& ctx) const
{
  auto& echain = m_path.find(ctx);

  // Create process model
  auto obj = m_data[score::StringConstant().Process].toObject();
  auto key = fromJsonValue<UuidKey<Process::ProcessModel>>(
      obj[score::StringConstant().uuid]);
  auto fac = ctx.app.interfaces<Process::ProcessFactoryList>().get(key);
  SCORE_ASSERT(fac);
  // TODO handle missing effect
  JSONObject::Deserializer des{obj};
  auto fx = fac->load(des.toVariant(), ctx, &echain);
  const auto ports = fx->findChildren<Process::Port*>();
  for (Process::Port* port : ports)
  {
    while (!port->cables().empty())
    {
      port->removeCable(port->cables().back());
    }
  }

  score::IDocument::changeObjectId(*fx, m_id);

  echain.insertEffect(fx, m_pos);
}

void LoadEffect::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_id << m_data << m_pos;
}

void LoadEffect::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_id >> m_data >> m_pos;
}

RemoveEffect::RemoveEffect(
    const Media::ChainProcess& model,
    const Process::ProcessModel& effect)
    : m_model{model}
    , m_id{effect.id()}
    , m_savedEffect{score::marshall<DataStream>(effect)}
{
  m_cables = Dataflow::saveCables(
      {(QObject*)&effect}, score::IDocument::documentContext(model));
  m_pos = model.effectPosition(effect.id());
}

void RemoveEffect::undo(const score::DocumentContext& ctx) const
{
  auto& process = m_model.find(ctx);
  auto& fact_list = ctx.app.interfaces<Process::ProcessFactoryList>();

  DataStream::Deserializer des{m_savedEffect};
  if (auto fx = deserialize_interface(fact_list, des, ctx, &process))
  {
    process.insertEffect(fx, m_pos);
  }
  else
  {
    SCORE_TODO;
    // Insert a fake effect ?
  }

  Dataflow::restoreCables(m_cables, ctx);
}

void RemoveEffect::redo(const score::DocumentContext& ctx) const
{
  Dataflow::removeCables(m_cables, ctx);

  auto& process = m_model.find(ctx);
  process.removeEffect(m_id);
}

void RemoveEffect::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_id << m_savedEffect << m_cables << m_pos;
}

void RemoveEffect::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_id >> m_savedEffect >> m_cables >> m_pos;
}

MoveEffect::MoveEffect(
    const Media::ChainProcess& effect,
    Id<Process::ProcessModel> id,
    int new_pos)
    : m_model{effect}, m_id{id}, m_newPos{new_pos}
{
  m_oldPos = effect.effectPosition(m_id);
}

void MoveEffect::undo(const score::DocumentContext& ctx) const
{
  auto& process = m_model.find(ctx);
  process.moveEffect(m_id, m_oldPos);
}

void MoveEffect::redo(const score::DocumentContext& ctx) const
{
  auto& process = m_model.find(ctx);
  process.moveEffect(m_id, m_newPos);
}

void MoveEffect::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_id << m_oldPos << m_newPos;
}

void MoveEffect::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_id >> m_oldPos >> m_newPos;
}
}
