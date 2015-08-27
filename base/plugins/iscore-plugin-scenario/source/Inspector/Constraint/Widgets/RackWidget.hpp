#pragma once
#include <QWidget>
#include <QMetaObject>

#include <QMap>

class QComboBox;

class ConstraintInspectorWidget;
class ConstraintModel;
class ConstraintViewModel;
class LambdaFriendlyQComboBox;

class RackWidget : public QWidget
{
        Q_OBJECT

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
