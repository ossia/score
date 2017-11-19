#include "FaustEffectModel.hpp"

template <>
void DataStreamReader::read(
        const Media::Effect::FaustEffectModel& eff)
{
    m_stream << eff.text();
    insertDelimiter();
}

template <>
void DataStreamWriter::write(
        Media::Effect::FaustEffectModel& eff)
{
    QString txt;
    m_stream >> txt;
    eff.setText(txt);
    checkDelimiter();
}

template <>
void JSONObjectReader::read(
        const Media::Effect::FaustEffectModel& eff)
{
    obj["Effect"] = eff.text();
}

template <>
void JSONObjectWriter::write(
        Media::Effect::FaustEffectModel& eff)
{
    eff.setText(obj["Effect"].toString());
}
