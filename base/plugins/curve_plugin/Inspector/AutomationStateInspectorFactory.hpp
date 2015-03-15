#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>


class AutomationStateInspectorFactory : public InspectorWidgetFactoryInterface
{
    public:
        AutomationStateInspectorFactory() :
            InspectorWidgetFactoryInterface {}
        {

        }

        virtual InspectorWidgetBase* makeWidget(QObject* sourceElement, QWidget* parent) override;

        virtual QList<QString> correspondingObjectsNames() const override
        {
            return {"AutomationState"};
        }
};
