#pragma once

#include <Inspector/InspectorWidgetFactoryInterface.hpp>

class StateInspectorFactory : public InspectorWidgetFactoryInterface
{
    public:
        QList<QString> correspondingObjectsNames() const override;
        InspectorWidgetBase* makeWidget(QObject* sourceElement) override;
};
