#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>


class TimeNodeInspectorFactory final : public InspectorWidgetFactory
{
    public:
        TimeNodeInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        virtual InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) override;

        virtual QList<QString> correspondingObjectsNames() const override;
};
