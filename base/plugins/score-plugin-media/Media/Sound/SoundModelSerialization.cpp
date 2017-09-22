#include "SoundModel.hpp"

template <>
void DataStreamReader::read(const Media::Sound::ProcessModel& proc)
{
    m_stream << proc.m_file.name();

    insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Sound::ProcessModel& proc)
{
    QString s;
    m_stream >> s;
    proc.setFile(s);

    checkDelimiter();
}

template <>
void JSONObjectReader::read(const Media::Sound::ProcessModel& proc)
{
    obj["File"] = proc.file().name();
}

template <>
void JSONObjectWriter::write(Media::Sound::ProcessModel& proc)
{
    proc.setFile(obj["File"].toString());
}
