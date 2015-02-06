#pragma once

#include <InspectorInterface/InspectorWidgetBase.hpp>

class AutomationModel;
class AutomationInspectorWidget : public InspectorWidgetBase
{
		Q_OBJECT
	public:
		explicit AutomationInspectorWidget (AutomationModel* object,
											QWidget* parent = 0);
};
