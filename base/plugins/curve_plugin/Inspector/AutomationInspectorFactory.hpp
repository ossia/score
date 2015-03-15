#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>


class AutomationInspectorFactory : public InspectorWidgetFactoryInterface
{
    public:
        AutomationInspectorFactory() :
            InspectorWidgetFactoryInterface {}
        {

        }

        virtual InspectorWidgetBase* makeWidget(QObject* sourceElement, QWidget* parent) override;

        virtual QList<QString> correspondingObjectsNames() const override
        {
            return {"Automation"};
        }
};
