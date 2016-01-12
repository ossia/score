#pragma once
#include <QWidget>

class LambdaFriendlyQComboBox;
class QString;

namespace Scenario
{
class ConstraintInspectorWidget;
class ConstraintModel;
class ConstraintViewModel;
class RackWidget final : public QWidget
{
    public:
        RackWidget(ConstraintInspectorWidget* parent);

        ~RackWidget();

        void viewModelsChanged();
        void updateComboBox(LambdaFriendlyQComboBox*, ConstraintViewModel* vm);

        static const QString hiddenText;


    private:
        QWidget* m_comboBoxesWidget{};
        const ConstraintModel& m_model;
        ConstraintInspectorWidget* m_parent {};

};
}
