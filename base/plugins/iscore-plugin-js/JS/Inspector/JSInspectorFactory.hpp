#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <JS/JSProcessMetadata.hpp>
#include <QList>
#include <QString>

class InspectorWidgetBase;
class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore


class JSInspectorFactory final : public InspectorWidgetFactory
{
    public:
        JSInspectorFactory();
        virtual ~JSInspectorFactory();

        InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                const iscore::DocumentContext& doc,
                QWidget* parent) const override;

        const QList<QString>& key_impl() const override
        {
            static const QList<QString> list{JSProcessMetadata::processObjectName()};
            return list;
        }
};
