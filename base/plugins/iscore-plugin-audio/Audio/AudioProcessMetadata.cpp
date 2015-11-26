#include "AudioProcessMetadata.hpp"
const ProcessFactoryKey& Audio::ProcessMetadata::factoryKey()
{
    static const ProcessFactoryKey name{"Audio"};
    return name;
}

QString Audio::ProcessMetadata::processObjectName()
{
    return "Audio";
}

QString Audio::ProcessMetadata::factoryPrettyName()
{
    return QObject::tr("Audio");
}
