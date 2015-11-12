#pragma once
#include <Process/ProcessFactoryKey.hpp>

struct AutomationProcessMetadata
{
        static const ProcessFactoryKey& factoryKey()
        {
            static const ProcessFactoryKey name{"Automation"};
            return name;
        }

        static QString processObjectName()
        {
            return "Automation";
        }

        static QString factoryPrettyName()
        {
            return QObject::tr("Automation");
        }
};
