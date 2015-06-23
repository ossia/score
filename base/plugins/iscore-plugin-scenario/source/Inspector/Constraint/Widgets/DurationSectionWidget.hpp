#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>

class QLabel;
class ConstraintModel;
class ConstraintInspectorWidget;
class QTimeEdit;
class QCheckBox;
class QGridLayout;
class QHBoxLayout;

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

        void on_modelDefaultDurationChanged(const TimeValue& dur);
        void on_modelMinDurationChanged(const TimeValue& dur);
        void on_modelMaxDurationChanged(const TimeValue& dur);
        void on_modelRigidityChanged(bool b);
        void on_durationsChanged();


    private:
        const ConstraintModel* m_model {};
        ConstraintInspectorWidget* m_parent {};

        QGridLayout* m_grid{};

        QLabel* m_maxLab{};
        QLabel* m_minLab{};

        QTimeEdit* m_minSpin{};
        QTimeEdit* m_valueSpin{};
        QTimeEdit* m_maxSpin{};

        QCheckBox* m_minNonNullBox{};
        QCheckBox* m_maxFiniteBox{};

        QCheckBox* m_infinite{};

        bool m_rigidity;

        TimeValue m_max;
        TimeValue m_default;

        SingleOngoingCommandDispatcher m_cmdDispatcher;
};
