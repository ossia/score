#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>


class ConstraintInspectorFactory : public InspectorWidgetFactory
{
    public:
        ConstraintInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        virtual InspectorWidgetBase* makeWidget(
                const QObject* sourceElement,
                QWidget* parent) override;

        virtual QList<QString> correspondingObjectsNames() const override
        {
            return {"ConstraintModel", "BaseConstraintModel"};
        }
};
