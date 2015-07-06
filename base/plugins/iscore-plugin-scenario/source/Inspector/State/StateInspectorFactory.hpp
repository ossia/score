#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>


class StateInspectorFactory : public InspectorWidgetFactory
{
    public:
        StateInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        virtual InspectorWidgetBase* makeWidget(
                const QObject* sourceElement,
                QWidget* parent) override;

        virtual QList<QString> correspondingObjectsNames() const override;
};
