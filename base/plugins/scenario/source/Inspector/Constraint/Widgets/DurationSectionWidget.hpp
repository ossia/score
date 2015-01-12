#include <InspectorInterface/InspectorSectionWidget.hpp>

class ConstraintModel;
class ConstraintInspectorWidget;
class DurationSectionWidget : public InspectorSectionWidget
{
		Q_OBJECT
	public:
		DurationSectionWidget(ConstraintInspectorWidget* parent);

	public slots:

		void minDurationSpinboxChanged(int val);
		void maxDurationSpinboxChanged(int val);
		void rigidCheckboxToggled(bool b);


	private:
		ConstraintModel* m_model{};
		ConstraintInspectorWidget* m_parent{};

		bool m_minSpinboxEditing{};
		bool m_maxSpinboxEditing{};
};
