#include <InspectorInterface/InspectorSectionWidget.hpp>
#include <ProcessInterface/TimeValue.hpp>

class QLabel;
class ConstraintModel;
class ConstraintInspectorWidget;
class DurationSectionWidget : public InspectorSectionWidget
{
        Q_OBJECT
    public:
        DurationSectionWidget(ConstraintInspectorWidget* parent);

    private slots:

        void minDurationSpinboxChanged(int val);
        void maxDurationSpinboxChanged(int val);
        void defaultDurationSpinboxChanged(int val);
        void rigidCheckboxToggled(bool b);

        void on_defaultDurationChanged(TimeValue dur);


    private:
        QLabel* m_valueLabel {};
        ConstraintModel* m_model {};
        ConstraintInspectorWidget* m_parent {};

        bool m_minSpinboxEditing {};
        bool m_maxSpinboxEditing {};
        bool m_valueSpinboxEditing {};
};
