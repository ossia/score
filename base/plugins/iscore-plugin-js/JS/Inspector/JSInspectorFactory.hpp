#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <JS/JSProcessMetadata.hpp>


class JSInspectorFactory final : public InspectorWidgetFactory
{
    public:
        JSInspectorFactory();
        virtual ~JSInspectorFactory();

        InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) override;

        const QList<QString>& key_impl() const override
        {
            static const QList<QString> list{JSProcessMetadata::processObjectName()};
            return list;
        }
};
