#include <InspectorInterface/InspectorSectionWidget.hpp>

class ConstraintModel;
class ConstraintInspectorWidget;
class DurationSectionWidget : public InspectorSectionWidget
{
	public:
		DurationSectionWidget(ConstraintInspectorWidget* parent);

	private:
		ConstraintModel* m_model{};
};