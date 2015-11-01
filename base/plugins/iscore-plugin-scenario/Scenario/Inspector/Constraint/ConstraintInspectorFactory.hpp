#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>


class ConstraintInspectorFactory final : public InspectorWidgetFactory
{
    public:
        ConstraintInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        virtual InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) override;

        virtual QList<QString> correspondingObjectsNames() const override
        {
            return {"ConstraintModel", "BaseConstraintModel"};
        }
};
