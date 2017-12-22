#include "EditFaustEffect.hpp"

#include <score/tools/IdentifierGeneration.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <Media/Effect/Faust/FaustEffectModel.hpp>
namespace Media::Commands
{

EditFaustEffect::EditFaustEffect(
        const Faust::FaustEffectModel& model,
        const QString& text):
    m_model{model},
    m_old{model.text()},
    m_new{text}
{
}

void EditFaustEffect::undo(const score::DocumentContext& ctx) const
{
    m_model.find(ctx).setText(m_old);
}

void EditFaustEffect::redo(const score::DocumentContext& ctx) const
{
    m_model.find(ctx).setText(m_new);
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
