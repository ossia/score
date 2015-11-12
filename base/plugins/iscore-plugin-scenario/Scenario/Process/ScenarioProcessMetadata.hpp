#pragma once
#include <Process/ProcessFactoryKey.hpp>

struct ScenarioProcessMetadata
{
        static const ProcessFactoryKey& factoryKey()
        {
            static const ProcessFactoryKey name{"Scenario"};
            return name;
        }

        static QString processObjectName()
        {
            return "Scenario";
        }

        static QString factoryPrettyName()
        {
            return QObject::tr("Scenario");
        }
};
