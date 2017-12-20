#include "InsertEffect.hpp"

#include <score/tools/IdentifierGeneration.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <Media/Effect/EffectProcessModel.hpp>
#include <Effect/EffectFactory.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

namespace Media
{
namespace Commands
{

InsertEffect::InsertEffect(
        const Effect::ProcessModel& model,
        const UuidKey<Process::EffectModel>& effectKind,
        const QString& text,
        std::size_t effectPos):
    m_model{model},
    m_id{getStrongId(model.effects())},
    m_effectKind{effectKind},
    m_effect{text},
    m_pos{effectPos}
{
}

void InsertEffect::undo(const score::DocumentContext& ctx) const
{
  auto& process = m_model.find(ctx);
  if(ossia::find(process.effects(), m_id) != process.effects().end())
    process.removeEffect(m_id);
 }

void InsertEffect::redo(const score::DocumentContext& ctx) const
{
    auto& process = m_model.find(ctx);
    auto& fact_list = ctx.app.interfaces<Process::EffectFactoryList>();

    if(auto fact = fact_list.get(m_effectKind))
    {
        auto model = fact->make(m_effect, m_id, &process);
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
    s << m_model << m_id << m_effectKind << m_effect << m_pos;
}

void InsertEffect::deserializeImpl(DataStreamOutput& s)
{
    s >> m_model >> m_id >> m_effectKind >> m_effect >> m_pos;
}




RemoveEffect::RemoveEffect(
        const Effect::ProcessModel& model,
        const Process::EffectModel& effect):
    m_model{model},
    m_id{effect.id()},
    m_savedEffect{score::marshall<DataStream>(effect)}
{
    m_pos = model.effectPosition(effect.id());
}

void RemoveEffect::undo(const score::DocumentContext& ctx) const
{
    auto& process = m_model.find(ctx);
    auto& fact_list = ctx.app.interfaces<Process::EffectFactoryList>();

    DataStream::Deserializer des{m_savedEffect};
    if(auto fx = deserialize_interface(fact_list, des, &process))
    {
        process.insertEffect(fx, m_pos);
    }
    else
    {
        SCORE_TODO;
        // Insert a fake effect ?
    }
}

void RemoveEffect::redo(const score::DocumentContext& ctx) const
{
    auto& process = m_model.find(ctx);
    process.removeEffect(m_id);
}

void RemoveEffect::serializeImpl(DataStreamInput& s) const
{
    s << m_model << m_id << m_savedEffect << m_pos;
}

void RemoveEffect::deserializeImpl(DataStreamOutput& s)
{
    s >> m_model >> m_id >> m_savedEffect >> m_pos;
}



MoveEffect::MoveEffect(
        const Effect::ProcessModel& effect,
        Id<Process::EffectModel> id,
        int new_pos):
    m_model{effect},
    m_id{id},
    m_newPos{new_pos}
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
}
