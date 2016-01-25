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

namespace Scenario
{
class TimeNodeInspectorFactory final : public Inspector::InspectorWidgetFactory
{
    public:
        TimeNodeInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        Inspector::InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                const iscore::DocumentContext& doc,
                QWidget* parent) const override;

        const QList<QString>& key_impl() const override;


        bool matches(const QObject& object) const override;
};
}
