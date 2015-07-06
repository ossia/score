#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>


class ScenarioInspectorFactory : public InspectorWidgetFactory
{
    public:
        ScenarioInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        virtual InspectorWidgetBase* makeWidget(
                const QObject* sourceElement,
                QWidget* parent) override;

        virtual QList<QString> correspondingObjectsNames() const override;
};
