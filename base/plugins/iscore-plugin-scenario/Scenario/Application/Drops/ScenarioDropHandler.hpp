#pragma once
#include <QPointF>
#include <QMimeData>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

namespace Scenario
{
class TemporalScenarioPresenter;
class DropHandler :
        public iscore::AbstractFactory<DropHandler>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                DropHandler,
                "ce1c5b6c-fe4c-416f-877c-eae642a1413a")
    public:
        virtual ~DropHandler();

        // Returns false if not handled.
        virtual bool handle(
                const TemporalScenarioPresenter&,
                QPointF drop,
                const QMimeData* mime) = 0;
};

class DropHandlerList final :
        public iscore::ConcreteFactoryList<DropHandler>
{
        public:
            virtual ~DropHandlerList();

        bool handle(const TemporalScenarioPresenter& scen,
                    QPointF drop,
                    const QMimeData* mime) const;
};
}
