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

        virtual InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) override;

        virtual const QList<QString>& key_impl() const override
        {
            static const QList<QString> lst{"Automation"};
            return lst;
        }
};
