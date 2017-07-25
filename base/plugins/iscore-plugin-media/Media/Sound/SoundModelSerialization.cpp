#include "SoundModel.hpp"

template <>
void DataStreamReader::read(const Media::Sound::ProcessModel& proc)
{
    m_stream << proc.m_file;

    insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Sound::ProcessModel& proc)
{
    m_stream >> proc.m_file;

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
    proc.m_file.load(obj["File"].toString());
}
