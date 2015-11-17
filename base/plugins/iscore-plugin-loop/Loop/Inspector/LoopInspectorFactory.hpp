#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Loop/LoopProcessMetadata.hpp>


class LoopInspectorFactory final : public InspectorWidgetFactory
{
    public:
        LoopInspectorFactory();
        virtual ~LoopInspectorFactory();

        InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) override;

        const QList<QString>& key_impl() const override
        {
            static const QList<QString> list{LoopProcessMetadata::processObjectName()};
            return list;
        }
};
