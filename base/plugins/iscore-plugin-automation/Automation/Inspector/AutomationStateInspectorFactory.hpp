#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <QList>

class QObject;
class QString;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore


namespace Automation
{
class StateInspectorFactory final :
        public Inspector::InspectorWidgetFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("71a5f5b6-6c10-4057-ab10-278c3f18e9af")
    public:
        StateInspectorFactory();

        Inspector::InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                const iscore::DocumentContext& doc,
                QWidget* parent) const override;

        bool matches(const QObject& object) const override;
};

}
