#pragma once
#include <Process/ProcessFactoryKey.hpp>

struct MappingProcessMetadata
{
        static const ProcessFactoryKey& factoryKey()
        {
            static const ProcessFactoryKey name{"Mapping"};
            return name;
        }

        static QString processObjectName()
        {
            return "Mapping";
        }

        static QString factoryPrettyName()
        {
            return QObject::tr("Mapping");
        }
};
