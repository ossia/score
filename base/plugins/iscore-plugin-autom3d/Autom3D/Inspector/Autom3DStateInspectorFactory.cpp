#include <QString>

#include "Autom3DStateInspector.hpp"
#include "Autom3DStateInspectorFactory.hpp"
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Autom3D/State/Autom3DState.hpp>
class InspectorWidgetBase;
class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Autom3D
{
StateInspectorFactory::StateInspectorFactory() :
    InspectorWidgetFactory {}
{

}

InspectorWidgetBase* StateInspectorFactory::makeWidget(
        const QObject& sourceElement,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    return new StateInspectorWidget{
                safe_cast<const ProcessState&>(sourceElement),
                doc,
                parent};
}

const QList<QString>&StateInspectorFactory::key_impl() const
{
    static const QList<QString> lst{"Autom3DState"};
    return lst;
}
}
