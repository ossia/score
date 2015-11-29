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

class CurvePointInspectorFactory final : public InspectorWidgetFactory
{
    public:
    CurvePointInspectorFactory() :
        InspectorWidgetFactory {}
    {

    }

    virtual InspectorWidgetBase* makeWidget(
        const QObject& sourceElement,
        iscore::Document& doc,
        QWidget* parent) override;

    virtual const QList<QString>& key_impl() const override;
};
