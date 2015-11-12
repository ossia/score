#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>


class AutomationInspectorFactory final : public InspectorWidgetFactory
{
    public:
        AutomationInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) override;

        const QList<QString>& key_impl() const override;
};
