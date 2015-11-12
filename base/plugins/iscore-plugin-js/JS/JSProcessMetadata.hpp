#pragma once
#include <Process/ProcessFactory.hpp>

struct JSProcessMetadata
{
        static const ProcessFactoryKey& factoryKey()
        {
            static const ProcessFactoryKey name{"Javascript"};
            return name;
        }

        static QString processObjectName()
        {
            return "Javascript";
        }

        static QString factoryPrettyName()
        {
            return QObject::tr("Javascript");
        }
};
