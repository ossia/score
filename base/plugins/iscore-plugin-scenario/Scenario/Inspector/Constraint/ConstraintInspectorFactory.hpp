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
        ISCORE_CONCRETE_FACTORY_DECL("1ca16c0a-6c01-4054-a646-d860a3886e81")
    public:
        ConstraintInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        Inspector::InspectorWidgetBase* makeWidget(
                const QList<const QObject*>& sourceElements,
                const iscore::DocumentContext& doc,
                QWidget* parent) const override;

        bool matches(const QList<const QObject*>& objects) const override;
};
}
