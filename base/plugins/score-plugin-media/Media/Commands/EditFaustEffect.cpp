#include "EditFaustEffect.hpp"

#include <Media/Effect/Faust/FaustEffectModel.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/IdentifierGeneration.hpp>
namespace Media
{

EditFaustEffect::EditFaustEffect(
    const Faust::FaustEffectModel& model, const QString& text)
    : m_model{model}
    ,
#if defined(HAS_FAUST)
    m_old{model.text()}
    ,
#endif
    m_new{text}
{
}

void EditFaustEffect::undo(const score::DocumentContext& ctx) const
{
#if defined(HAS_FAUST)
  m_model.find(ctx).setText(m_old);
#endif
}

void EditFaustEffect::redo(const score::DocumentContext& ctx) const
{
#if defined(HAS_FAUST)
  m_model.find(ctx).setText(m_new);
#endif
}

void EditFaustEffect::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_old << m_new;
}

void EditFaustEffect::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_old >> m_new;
}
}
