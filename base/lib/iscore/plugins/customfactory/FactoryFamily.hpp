#pragma once
#include <QObject>
#include <functional>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>


namespace iscore
{
    class FactoryInterfaceBase;

    /**
     * @brief The FactoryFamily class
     *
     * Keeps the factories, so that they can be found easily.
     */
    struct FactoryFamily
    {
        // Example : InspectorWidgetFactory
        iscore::FactoryBaseKey name;

        // This function is called whenever a new factory interface is added to this family.
        std::function<void (iscore::FactoryInterfaceBase*)> onInstantiation;

        // The factories that correspond to this CustomFactoryInterface, and
        // are registered by subsequent plugins.
        // Example : IntervalInspectorFactory, EventInspectorFactory.
    };
}
