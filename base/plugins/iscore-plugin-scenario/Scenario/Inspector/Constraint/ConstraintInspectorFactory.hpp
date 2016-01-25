#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <QList>

class QObject;
class QString;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Scenario
{
class ConstraintInspectorFactory final : public Inspector::InspectorWidgetFactory
{
    public:
        ConstraintInspectorFactory() :
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
