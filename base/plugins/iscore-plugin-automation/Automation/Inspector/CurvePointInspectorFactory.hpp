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

namespace Automation
{
class PointInspectorFactory final : public InspectorWidgetFactory
{
    public:
    PointInspectorFactory() :
        InspectorWidgetFactory {}
    {

    }

    virtual InspectorWidgetBase* makeWidget(
        const QObject& sourceElement,
        const iscore::DocumentContext& doc,
        QWidget* parent) const override;

    virtual const QList<QString>& key_impl() const override;
};
}
