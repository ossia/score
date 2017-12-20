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

}
}
