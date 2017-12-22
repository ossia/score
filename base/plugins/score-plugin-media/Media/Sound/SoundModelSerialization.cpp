#include "SoundModel.hpp"

template <>
void DataStreamReader::read(const Media::Sound::ProcessModel& proc)
{
    m_stream << proc.m_file.name() << *proc.outlet << proc.m_upmixChannels << proc.m_startChannel;

    insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Sound::ProcessModel& proc)
{
    QString s;
    m_stream >> s;
    proc.setFile(s);
    proc.outlet = make_outlet(*this, &proc);

    m_stream >> proc.m_upmixChannels >> proc.m_startChannel;
    checkDelimiter();
}

template <>
void JSONObjectReader::read(const Media::Sound::ProcessModel& proc)
{
    obj["File"] = proc.file().name();
    obj["Outlet"] = toJsonObject(*proc.outlet);
    obj["Upmix"] = proc.m_upmixChannels;
    obj["Start"] = proc.m_startChannel;
}

template <>
void JSONObjectWriter::write(Media::Sound::ProcessModel& proc)
{
    proc.setFile(obj["File"].toString());
    JSONObjectWriter writer{obj["Outlet"].toObject()};
    proc.outlet = Process::make_outlet(writer, &proc);
    proc.m_upmixChannels = obj["Upmix"].toInt();
    proc.m_startChannel  = obj["Start"].toInt();
}
