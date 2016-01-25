#pragma once
#include <Process/ProcessFactoryKey.hpp>

struct ScenarioProcessMetadata
{
        static const ProcessFactoryKey& abstractFactoryKey()
        {
            static const ProcessFactoryKey name{"Scenario"};
            return name;
        }

        static QString processObjectName()
        {
            return "Scenario";
        }

        static QString factorydescription()
        {
            return QObject::tr("Scenario");
        }
};
