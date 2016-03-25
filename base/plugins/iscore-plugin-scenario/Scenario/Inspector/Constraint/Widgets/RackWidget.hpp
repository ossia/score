#pragma once
#include <QWidget>

class QComboBox;
class QString;

namespace Scenario
{
class ProcessViewTabWidget;
class ConstraintModel;
class ConstraintViewModel;
class RackWidget final : public QWidget
{
    public:
        RackWidget(ProcessViewTabWidget* parentTabWidget, QWidget* parent);

        ~RackWidget();

        void viewModelsChanged();
        void updateComboBox(QComboBox*, ConstraintViewModel* vm);

        static const QString hiddenText;


    private:
        QWidget* m_comboBoxesWidget{};
        const ConstraintModel& m_model;
        ProcessViewTabWidget* m_parent {};

};
}
