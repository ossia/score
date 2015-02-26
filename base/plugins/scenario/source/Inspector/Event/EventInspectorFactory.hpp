#pragma once
#include <QObject>
#include <InspectorInterface/InspectorWidgetFactoryInterface.hpp>


class EventInspectorFactory : public InspectorWidgetFactoryInterface
{
    public:
        EventInspectorFactory() :
            InspectorWidgetFactoryInterface {}
        {

        }

        virtual InspectorWidgetBase* makeWidget(QObject* sourceElement) override;
        virtual InspectorWidgetBase* makeWidget(QList<QObject*> sourceElements) override;

        virtual QList<QString> correspondingObjectsNames() const override
        {
            return {"EventModel"};
        }
};
