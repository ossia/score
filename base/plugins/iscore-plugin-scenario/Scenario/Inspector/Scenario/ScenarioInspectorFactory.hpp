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

class ScenarioInspectorFactory final : public InspectorWidgetFactory
{
    public:
        ScenarioInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                const iscore::DocumentContext& doc,
                QWidget* parent) const override;

        const QList<QString>& key_impl() const override;

};
