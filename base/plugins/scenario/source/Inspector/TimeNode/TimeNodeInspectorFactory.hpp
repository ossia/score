#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>


class TimeNodeInspectorFactory : public InspectorWidgetFactoryInterface
{
    public:
        TimeNodeInspectorFactory() :
            InspectorWidgetFactoryInterface {}
        {

        }

        virtual InspectorWidgetBase* makeWidget(
                const QObject* sourceElement,
                QWidget* parent) override;

        virtual QList<QString> correspondingObjectsNames() const override
        {
            return {"TimeNodeModel"};
        }
};
