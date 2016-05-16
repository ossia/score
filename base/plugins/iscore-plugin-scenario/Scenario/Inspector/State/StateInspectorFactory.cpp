#include <Scenario/Document/State/StateModel.hpp>
#include <QString>

#include "StateInspectorFactory.hpp"
#include "StateInspectorWidget.hpp"

class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Scenario
{
Inspector::InspectorWidgetBase* StateInspectorFactory::makeWidget(
        const QList<const QObject*>& sourceElements,
        const iscore::DocumentContext& doc,
        QWidget* parentWidget) const
{
    auto baseW = new Inspector::InspectorWidgetBase{
                 static_cast<const StateModel&>(*sourceElements.first()),
                 doc,
                 parentWidget};
    /*
    auto contentW = new StateInspectorWidget{
                    static_cast<const StateModel&>(*sourceElements.first()),
                            doc,
                            parentWidget};
*/
//    baseW->updateAreaLayout({contentW});
    return baseW;
}

bool StateInspectorFactory::matches(
        const QList<const QObject*>& objects) const
{
    return dynamic_cast<const StateModel*>(objects.first());
}
}
