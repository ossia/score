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
        void setModel(const ConstraintModel*);

        static const QString hiddenText;


    private:
        QWidget* m_comboBoxesWidget{};
        const ConstraintModel* m_model {};
        ConstraintInspectorWidget* m_parent {};

};
