#include "SoundModel.hpp"

template <>
void DataStreamReader::read(const Media::Sound::ProcessModel& proc)
{
    m_stream << proc.m_file.name() << *proc.outlet;

    insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Sound::ProcessModel& proc)
{
    QString s;
    m_stream >> s;
    proc.setFile(s);
    proc.outlet = make_outlet(*this, &proc);

    checkDelimiter();
}

template <>
void JSONObjectReader::read(const Media::Sound::ProcessModel& proc)
{
    obj["File"] = proc.file().name();
    obj["Outlet"] = toJsonObject(*proc.outlet);
}

template <>
void JSONObjectWriter::write(Media::Sound::ProcessModel& proc)
{
    proc.setFile(obj["File"].toString());
    JSONObjectWriter writer{obj["Outlet"].toObject()};
    proc.outlet = Process::make_outlet(writer, &proc);
}
