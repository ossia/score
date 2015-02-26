#pragma once

#include <InspectorInterface/InspectorWidgetBase.hpp>

class ScenarioModel;
class ScenarioInspectorWidget : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit ScenarioInspectorWidget (ScenarioModel* object,
                                          QWidget* parent = 0);
};
