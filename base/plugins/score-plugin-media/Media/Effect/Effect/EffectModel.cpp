#include "EffectModel.hpp"
#include <Media/Effect/Effect/EffectParameters.hpp>
namespace Media
{
namespace Effect
{
EffectModel::EffectModel(
        const Id<EffectModel>& id,
        QObject* parent):
    Entity{id, staticMetaObject.className(), parent}
{
    metadata().setInstanceName(*this);
}

EffectModel::EffectModel(
        const EffectModel& source,
        const Id<EffectModel>& id,
        QObject* parent):
    Entity{source, id, staticMetaObject.className(), parent}
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
}
}

template <>
void DataStreamReader::read(
        const Media::Effect::EffectModel& eff)
{
}

template <>
void DataStreamWriter::write(
        Media::Effect::EffectModel& eff)
{
}

template <>
void JSONObjectReader::read(
        const Media::Effect::EffectModel& eff)
{
}

template <>
void JSONObjectWriter::write(
        Media::Effect::EffectModel& eff)
{
}
