#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>

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
