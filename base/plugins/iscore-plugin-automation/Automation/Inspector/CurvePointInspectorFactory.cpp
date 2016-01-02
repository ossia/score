#include <QString>

#include "CurvePointInspectorFactory.hpp"
#include "CurvePointInspectorWidget.hpp"
#include <Curve/Point/CurvePointModel.hpp>
class InspectorWidgetBase;
class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore


namespace Automation
{
InspectorWidgetBase* PointInspectorFactory::makeWidget(
        const QObject& sourceElement,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    return new PointInspectorWidget{
        safe_cast<const Curve::PointModel&>(sourceElement),
                doc,
                parent};
}

const QList<QString>&PointInspectorFactory::key_impl() const
{
    static const QList<QString>& lst{"CurvePointModel"};
    return lst;
}
}
