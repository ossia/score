#include <InspectorInterface/InspectorSectionWidget.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <core/presenter/command/OngoingCommandManager.hpp>

class QLabel;
class ConstraintModel;
class ConstraintInspectorWidget;
class QTimeEdit;

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

        void on_modelDefaultDurationChanged(TimeValue dur);
        void on_modelMinDurationChanged(TimeValue dur);
        void on_modelMaxDurationChanged(TimeValue dur);


    private:
        QLabel* m_valueLabel {};
        ConstraintModel* m_model {};
        ConstraintInspectorWidget* m_parent {};

        QTimeEdit* m_minSpin{};
        QTimeEdit* m_valueSpin{};
        QTimeEdit* m_maxSpin{};

        OngoingCommandDispatcher<MergeStrategy::Simple>* m_cmdManager{};
};
