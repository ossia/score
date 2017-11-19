#include "EnvelopeEffectModel.hpp"
#include <QUrl>
#include <QFile>

namespace Media
{
namespace Effect
{

EnvelopeEffectModel::EnvelopeEffectModel(
        const QString& path,
        const Id<EffectModel>& id,
        QObject* parent):
    EffectModel{id, parent}
{
  init();
}

EnvelopeEffectModel::EnvelopeEffectModel(
        const EnvelopeEffectModel& source,
        const Id<EffectModel>& id,
        QObject* parent):
    EffectModel{id, parent}
{
  init();
}

void EnvelopeEffectModel::init()
{
  metadata().setLabel(tr("Envelope"));
}
}
}

template <>
void DataStreamReader::read(
    const Media::Effect::EnvelopeEffectModel& eff)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(
    Media::Effect::EnvelopeEffectModel& eff)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(
    const Media::Effect::EnvelopeEffectModel& eff)
{
}

template <>
void JSONObjectWriter::write(
    Media::Effect::EnvelopeEffectModel& eff)
{
}
