#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <JS/JSProcessModel.hpp>


class JSInspectorFactory final : public InspectorWidgetFactory
{
    public:
        JSInspectorFactory();
        virtual ~JSInspectorFactory();

        InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) override;

        QList<QString> key_impl() const override
        {
            return {JSProcessModel::staticProcessName()}; // TODO use this everywhere. (automation, mapping ...)
        }
};
