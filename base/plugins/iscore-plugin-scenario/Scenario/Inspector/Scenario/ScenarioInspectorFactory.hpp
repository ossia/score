#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>


class ScenarioInspectorFactory final : public InspectorWidgetFactory
{
    public:
        ScenarioInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        virtual InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) override;

        virtual QList<QString> correspondingObjectsNames() const override;
};
