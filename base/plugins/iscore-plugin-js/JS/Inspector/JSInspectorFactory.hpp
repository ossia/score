#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <JS/JSProcessMetadata.hpp>
#include <qlist.h>
#include <qstring.h>

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
                iscore::Document& doc,
                QWidget* parent) override;

        const QList<QString>& key_impl() const override
        {
            static const QList<QString> list{JSProcessMetadata::processObjectName()};
            return list;
        }
};
