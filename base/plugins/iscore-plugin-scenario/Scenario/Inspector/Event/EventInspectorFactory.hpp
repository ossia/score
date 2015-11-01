#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>


class EventInspectorFactory final : public InspectorWidgetFactory
{
    public:
        EventInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        virtual InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) override;

        virtual QList<QString> correspondingObjectsNames() const override;
};
