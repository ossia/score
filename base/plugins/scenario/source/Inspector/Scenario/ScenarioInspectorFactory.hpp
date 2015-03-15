#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>


class ScenarioInspectorFactory : public InspectorWidgetFactoryInterface
{
    public:
        ScenarioInspectorFactory() :
            InspectorWidgetFactoryInterface {}
        {

        }

        virtual InspectorWidgetBase* makeWidget(QObject* sourceElement) override;

        virtual QList<QString> correspondingObjectsNames() const override
        {
            return {"Scenario"};
        }
};
