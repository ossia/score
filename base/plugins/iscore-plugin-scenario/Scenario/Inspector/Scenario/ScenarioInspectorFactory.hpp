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

        InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) override;

        const QList<QString>& key_impl() const override;

};
