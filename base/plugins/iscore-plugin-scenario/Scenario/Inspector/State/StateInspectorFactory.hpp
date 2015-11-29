#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <QList>

class InspectorWidgetBase;
class QObject;
class QString;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore


class StateInspectorFactory final : public InspectorWidgetFactory
{
    public:
        StateInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        virtual InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) override;

        const QList<QString>& key_impl() const override;
};
