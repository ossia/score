#include "MissingEffectModel.hpp"
namespace Media
{
namespace Effect
{

MissingEffectModel::MissingEffectModel(
        const QByteArray& MissingProgram,
        const Id<Process::EffectModel>& id,
        QObject* parent):
    Process::EffectModel{id, parent}
{
    SCORE_TODO;
}

}
}
