#include <QWidget>

class QComboBox;

class ConstraintInspectorWidget;
class ConstraintModel;
class AbstractConstraintViewModel;
class LambdaFriendlyQComboBox;
class BoxWidget : public QWidget
{
        Q_OBJECT

    public:
        BoxWidget(ConstraintInspectorWidget* parent);

        void viewModelsChanged();
        void updateComboBox(LambdaFriendlyQComboBox*, AbstractConstraintViewModel* vm);
        void setModel(ConstraintModel*);

        const QString hiddenText
        {
            tr("Hide")
        };

    public slots:
        void on_comboBoxActivated(QString);

    private:
        QWidget* m_comboBoxesWidget{};
        QList<QComboBox*> m_comboBoxes {};
        ConstraintModel* m_model {};
        ConstraintInspectorWidget* m_parent {};

};
