#include <QWidget>
#include <QMetaObject>

#include <QMap>

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

        ~BoxWidget();

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
        ConstraintModel* m_model {};
        ConstraintInspectorWidget* m_parent {};

        QMap<AbstractConstraintViewModel*, QMetaObject::Connection> m_connections;

};
