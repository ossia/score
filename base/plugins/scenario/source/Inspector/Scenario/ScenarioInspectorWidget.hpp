#pragma once

#include <InspectorInterface/InspectorWidgetBase.hpp>

class ScenarioProcessSharedModel;
class ScenarioInspectorWidget : public InspectorWidgetBase
{
		Q_OBJECT
	public:
		explicit ScenarioInspectorWidget (ScenarioProcessSharedModel* object,
										  QWidget* parent = 0);
};
