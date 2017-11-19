#include "MissingEffectModel.hpp"
namespace Media
{
namespace Effect
{

MissingEffectModel::MissingEffectModel(
        const QByteArray& MissingProgram,
        const Id<EffectModel>& id,
        QObject* parent):
    EffectModel{id, parent}
{
    SCORE_TODO;
}

MissingEffectModel::MissingEffectModel(
        const MissingEffectModel& source,
        const Id<EffectModel>& id,
        QObject* parent):
    EffectModel{id, parent}
{
    SCORE_TODO;
}

MissingEffectModel*MissingEffectModel::clone(const Id<EffectModel>& newId, QObject* parent) const
{
    SCORE_TODO;
    return nullptr;
}

}
}
