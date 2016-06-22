#pragma once
#include <QPointF>
#include <QMimeData>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
class TemporalScenarioPresenter;
class ISCORE_PLUGIN_SCENARIO_EXPORT DropHandler :
        public iscore::AbstractFactory<DropHandler>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                DropHandler,
                "ce1c5b6c-fe4c-416f-877c-eae642a1413a")
    public:
        virtual ~DropHandler();

        // Returns false if not handled.
        virtual bool handle(
                const Scenario::TemporalScenarioPresenter&,
                QPointF pos,
                const QMimeData* mime) = 0;
};

class DropHandlerList final :
        public iscore::ConcreteFactoryList<DropHandler>
{
        public:
            virtual ~DropHandlerList();

        bool handle(const TemporalScenarioPresenter& scen,
                    QPointF pos,
                    const QMimeData* mime) const;
};
}
