#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <iscore/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include "iscore/widgets/SpinBoxes.hpp"

class QLabel;
class ConstraintModel;
class ConstraintInspectorWidget;
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

        void on_minNonNullToggled(bool val);
        void on_maxFiniteToggled(bool val);

    private:
        const ConstraintModel& m_model;
        ConstraintInspectorWidget* m_parent {};

        QGridLayout* m_grid{};

        QLabel* m_maxLab{};
        QLabel* m_minLab{};

        iscore::TimeSpinBox* m_minSpin{};
        iscore::TimeSpinBox* m_valueSpin{};
        iscore::TimeSpinBox* m_maxSpin{};

        QCheckBox* m_minNonNullBox{};
        QCheckBox* m_maxFiniteBox{};

        TimeValue m_max;
        TimeValue m_min;

        OngoingCommandDispatcher m_dispatcher;
};
