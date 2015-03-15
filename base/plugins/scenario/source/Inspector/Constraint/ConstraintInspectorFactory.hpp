#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>


class ConstraintInspectorFactory : public InspectorWidgetFactoryInterface
{
    public:
        ConstraintInspectorFactory() :
            InspectorWidgetFactoryInterface {}
        {

        }

        virtual InspectorWidgetBase* makeWidget(QObject* sourceElement) override;

        virtual QList<QString> correspondingObjectsNames() const override
        {
            return {"ConstraintModel", "BaseConstraintModel"};
        }
};
