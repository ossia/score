#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>


class AutomationStateInspectorFactory final : public InspectorWidgetFactory
{
    public:
        AutomationStateInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        virtual InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) override;

        virtual const QList<QString>& key_impl() const override
        {
            static const QList<QString> lst{"AutomationState"};
            return lst;
        }
};
