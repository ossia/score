#include <QString>

#include "CurvePointInspectorFactory.hpp"
#include "CurvePointInspectorWidget.hpp"
#include <Curve/Point/CurvePointModel.hpp>
class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore


namespace Automation
{
Inspector::InspectorWidgetBase* PointInspectorFactory::makeWidget(
        const QList<const QObject*>&  sourceElements,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    return new PointInspectorWidget{
        safe_cast<const Curve::PointModel&>(*sourceElements.first()),
                doc,
                parent};
}

bool PointInspectorFactory::matches(
        const QList<const QObject*>& objects) const
{
    return dynamic_cast<const Curve::PointModel*>(objects.first());
}
}
