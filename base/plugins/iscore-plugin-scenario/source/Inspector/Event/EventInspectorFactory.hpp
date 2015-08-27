#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>


class EventInspectorFactory : public InspectorWidgetFactory
{
    public:
        EventInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        virtual InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                QWidget* parent) override;

        virtual QList<QString> correspondingObjectsNames() const override;
};
