#include <Inspector/InspectorSectionWidget.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <iscore/command/OngoingCommandManager.hpp>

class QLabel;
class ConstraintModel;
class ConstraintInspectorWidget;
class QTimeEdit;
class QCheckBox;
class DurationSectionWidget : public InspectorSectionWidget
{
        Q_OBJECT
    public:
        DurationSectionWidget(ConstraintInspectorWidget* parent);


    virtual ~DurationSectionWidget();

    private slots:

        void minDurationSpinboxChanged(int val);
        void maxDurationSpinboxChanged(int val);
        void defaultDurationSpinboxChanged(int val);
        void rigidCheckboxToggled(bool b);

        void on_modelDefaultDurationChanged(TimeValue dur);
        void on_modelMinDurationChanged(TimeValue dur);
        void on_modelMaxDurationChanged(TimeValue dur);
        void on_durationsChanged();


    private:
        QLabel* m_valueLabel {};
        ConstraintModel* m_model {};
        ConstraintInspectorWidget* m_parent {};

        QTimeEdit* m_minSpin{};
        QTimeEdit* m_valueSpin{};
        QTimeEdit* m_maxSpin{};
        QCheckBox* m_infinite{};

        TimeValue m_min;
        TimeValue m_max;
        TimeValue m_default;

        OngoingCommandDispatcher<MergeStrategy::Simple>*m_cmdDispatcher{};
};
